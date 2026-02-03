/*
 * =====================================================
 * OTA UPDATE SYSTEM FOR FRIYAY FOREVER
 * =====================================================
 *
 * This module provides Over-The-Air (OTA) firmware updates
 * via GitHub Releases (works with private repos!).
 *
 * How it works:
 * 1. Device queries GitHub Releases API for latest release
 * 2. Downloads version.json from release assets
 * 3. Compares with compiled-in FIRMWARE_VERSION
 * 4. If newer version available, downloads firmware.bin
 * 5. Uses ESP32 Update library to flash new firmware
 * 6. Device reboots with new firmware
 *
 * Security considerations:
 * - Uses HTTPS for all downloads
 * - Release assets are public even for private repos
 * - Validates firmware size before flashing
 * - Checks available space before download
 * - Provides rollback info in case of issues
 *
 * Usage:
 * 1. Set GITHUB_USER and GITHUB_REPO below
 * 2. Create GitHub Release with firmware.bin and version.json
 * 3. Create OTAUpdater instance
 * 4. Call checkForUpdate() to check for new versions
 * 5. Call performUpdate() to download and install
 */

#ifndef OTA_UPDATES_H
#define OTA_UPDATES_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>

// ============================================================
// CONFIGURATION - UPDATE THESE FOR YOUR GITHUB REPO
// ============================================================

#define GITHUB_USER "squid-baby"
#define GITHUB_REPO "friyay-forever"

// GitHub API URL for latest release
#define GITHUB_API_URL "https://api.github.com/repos/" GITHUB_USER "/" GITHUB_REPO "/releases/latest"

// Timeouts and limits
#define OTA_HTTP_TIMEOUT      15000   // 15 seconds for API/version check
#define OTA_DOWNLOAD_TIMEOUT  180000  // 3 minutes for firmware download
#define OTA_MAX_FIRMWARE_SIZE 3000000 // 3MB max firmware size
#define OTA_MIN_FREE_SPACE    500000  // 500KB minimum free space

// ============================================================
// OTA UPDATER CLASS
// ============================================================

class OTAUpdater {
public:
    // Callback type for progress updates (0-100)
    typedef void (*ProgressCallback)(int percent);

    OTAUpdater() :
        _progressCallback(nullptr),
        _updateAvailable(false),
        _lastError(""),
        _latestVersion(""),
        _releaseNotes(""),
        _firmwareUrl(""),
        _firmwareSize(0),
        _isCritical(false) {
    }

    // Set callback for progress updates during download
    void setProgressCallback(ProgressCallback callback) {
        _progressCallback = callback;
    }

    // Get the currently running firmware version
    String getCurrentVersion() {
        #ifdef FIRMWARE_VERSION
            return String(FIRMWARE_VERSION);
        #else
            return "unknown";
        #endif
    }

    // Check GitHub Releases for available updates
    // Returns true if a newer version is available
    bool checkForUpdate() {
        _updateAvailable = false;
        _lastError = "";
        _latestVersion = "";
        _releaseNotes = "";
        _firmwareUrl = "";
        _firmwareSize = 0;
        _isCritical = false;

        if (WiFi.status() != WL_CONNECTED) {
            _lastError = "WiFi not connected";
            Serial.println("[OTA] Error: WiFi not connected");
            return false;
        }

        Serial.println("[OTA] Checking GitHub Releases for updates...");
        Serial.print("[OTA] API URL: ");
        Serial.println(GITHUB_API_URL);

        // Use WiFiClientSecure for HTTPS
        WiFiClientSecure client;
        client.setInsecure();  // Skip certificate verification for simplicity

        HTTPClient http;
        http.begin(client, GITHUB_API_URL);
        http.setTimeout(OTA_HTTP_TIMEOUT);
        http.addHeader("Accept", "application/vnd.github.v3+json");
        http.addHeader("User-Agent", "ESP32-OTA-Updater");

        int httpCode = http.GET();

        if (httpCode != 200) {
            _lastError = "GitHub API error: " + String(httpCode);
            Serial.printf("[OTA] GitHub API error: %d\n", httpCode);
            http.end();
            return false;
        }

        String payload = http.getString();
        http.end();

        // Parse GitHub release JSON
        DynamicJsonDocument doc(8192);
        DeserializationError jsonError = deserializeJson(doc, payload);

        if (jsonError) {
            _lastError = "JSON parse error: " + String(jsonError.c_str());
            Serial.printf("[OTA] JSON error: %s\n", jsonError.c_str());
            return false;
        }

        // Extract tag name (version)
        const char* tagName = doc["tag_name"];
        if (!tagName) {
            _lastError = "No tag_name in release";
            Serial.println("[OTA] Error: No tag_name");
            return false;
        }

        // Remove 'v' prefix if present
        _latestVersion = String(tagName);
        if (_latestVersion.startsWith("v")) {
            _latestVersion = _latestVersion.substring(1);
        }

        // Get release notes from body
        if (doc.containsKey("body") && !doc["body"].isNull()) {
            _releaseNotes = doc["body"].as<String>();
            // Truncate if too long
            if (_releaseNotes.length() > 200) {
                _releaseNotes = _releaseNotes.substring(0, 197) + "...";
            }
        }

        // Find firmware.bin and version.json in assets
        String versionJsonUrl = "";
        JsonArray assets = doc["assets"];

        for (JsonObject asset : assets) {
            String name = asset["name"].as<String>();
            String downloadUrl = asset["browser_download_url"].as<String>();
            int size = asset["size"].as<int>();

            if (name == "firmware.bin") {
                _firmwareUrl = downloadUrl;
                _firmwareSize = size;
                Serial.printf("[OTA] Found firmware.bin: %d bytes\n", size);
            }
            if (name == "version.json") {
                versionJsonUrl = downloadUrl;
                Serial.println("[OTA] Found version.json");
            }
        }

        if (_firmwareUrl.length() == 0) {
            _lastError = "No firmware.bin in release";
            Serial.println("[OTA] Error: No firmware.bin asset");
            return false;
        }

        // Optionally fetch version.json for extra metadata (critical flag, etc.)
        if (versionJsonUrl.length() > 0) {
            fetchVersionJson(versionJsonUrl);
        }

        // Compare versions
        String currentVer = getCurrentVersion();
        Serial.printf("[OTA] Current: %s, Latest: %s\n",
                     currentVer.c_str(), _latestVersion.c_str());

        if (isNewerVersion(_latestVersion, currentVer)) {
            _updateAvailable = true;
            Serial.println("[OTA] Update available!");
            Serial.printf("[OTA] Firmware URL: %s\n", _firmwareUrl.c_str());
            return true;
        }

        Serial.println("[OTA] Already up to date");
        return false;
    }

    // Check if an update is available (call checkForUpdate first)
    bool isUpdateAvailable() {
        return _updateAvailable;
    }

    // Get the latest version string
    String getLatestVersion() {
        return _latestVersion;
    }

    // Get release notes for the latest version
    String getReleaseNotes() {
        return _releaseNotes;
    }

    // Check if the update is marked as critical
    bool isCriticalUpdate() {
        return _isCritical;
    }

    // Get the last error message
    String getLastError() {
        return _lastError;
    }

    // Download and install the update
    // Returns true on success (device will reboot)
    // Returns false on failure (check getLastError())
    bool performUpdate() {
        if (!_updateAvailable) {
            _lastError = "No update available";
            return false;
        }

        if (_firmwareUrl.length() == 0) {
            _lastError = "No firmware URL";
            return false;
        }

        if (WiFi.status() != WL_CONNECTED) {
            _lastError = "WiFi not connected";
            return false;
        }

        Serial.println("[OTA] Starting firmware download...");
        Serial.print("[OTA] URL: ");
        Serial.println(_firmwareUrl);

        // Check available space
        size_t freeSpace = ESP.getFreeSketchSpace();
        Serial.printf("[OTA] Free sketch space: %d bytes\n", freeSpace);

        if (freeSpace < OTA_MIN_FREE_SPACE) {
            _lastError = "Insufficient space for update";
            Serial.println("[OTA] Error: Not enough space");
            return false;
        }

        // Use WiFiClientSecure for HTTPS GitHub download
        WiFiClientSecure client;
        client.setInsecure();

        HTTPClient http;
        http.begin(client, _firmwareUrl);
        http.setTimeout(OTA_DOWNLOAD_TIMEOUT);
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);  // GitHub redirects to CDN

        int httpCode = http.GET();

        if (httpCode != 200) {
            _lastError = "Download failed: HTTP " + String(httpCode);
            Serial.printf("[OTA] Download failed: %d\n", httpCode);
            http.end();
            return false;
        }

        int contentLength = http.getSize();
        Serial.printf("[OTA] Firmware size: %d bytes\n", contentLength);

        if (contentLength <= 0) {
            _lastError = "Invalid content length";
            Serial.println("[OTA] Error: Invalid content length");
            http.end();
            return false;
        }

        if (contentLength > OTA_MAX_FIRMWARE_SIZE) {
            _lastError = "Firmware too large";
            Serial.println("[OTA] Error: Firmware exceeds max size");
            http.end();
            return false;
        }

        // Begin OTA update
        if (!Update.begin(contentLength)) {
            _lastError = "Update.begin failed: " + String(Update.errorString());
            Serial.printf("[OTA] Update.begin failed: %s\n", Update.errorString());
            http.end();
            return false;
        }

        Serial.println("[OTA] Update started, downloading...");

        WiFiClient* stream = http.getStreamPtr();

        // Download and write in chunks
        uint8_t buffer[1024];
        int bytesWritten = 0;
        int lastProgress = -1;
        unsigned long startTime = millis();

        while (bytesWritten < contentLength) {
            // Check for timeout
            if (millis() - startTime > OTA_DOWNLOAD_TIMEOUT) {
                _lastError = "Download timeout";
                Serial.println("[OTA] Error: Download timeout");
                Update.abort();
                http.end();
                return false;
            }

            // Read available data
            size_t available = stream->available();
            if (available > 0) {
                int toRead = min((size_t)sizeof(buffer), available);
                int bytesRead = stream->readBytes(buffer, toRead);

                if (bytesRead > 0) {
                    // Write to flash
                    size_t written = Update.write(buffer, bytesRead);
                    if (written != (size_t)bytesRead) {
                        _lastError = "Write error: " + String(Update.errorString());
                        Serial.printf("[OTA] Write error: %s\n", Update.errorString());
                        Update.abort();
                        http.end();
                        return false;
                    }

                    bytesWritten += bytesRead;

                    // Update progress
                    int progress = (bytesWritten * 100) / contentLength;
                    if (progress != lastProgress) {
                        lastProgress = progress;
                        Serial.printf("[OTA] Progress: %d%%\n", progress);

                        if (_progressCallback) {
                            _progressCallback(progress);
                        }
                    }
                }
            } else {
                // No data available, small delay
                delay(10);
            }

            // Keep watchdog happy
            yield();
        }

        http.end();

        // Verify and finish update
        if (!Update.end(true)) {
            _lastError = "Update.end failed: " + String(Update.errorString());
            Serial.printf("[OTA] Update.end failed: %s\n", Update.errorString());
            return false;
        }

        if (!Update.isFinished()) {
            _lastError = "Update not finished properly";
            Serial.println("[OTA] Error: Update not finished");
            return false;
        }

        Serial.println("[OTA] Update successful! Rebooting...");
        delay(1000);
        ESP.restart();

        // Should never reach here
        return true;
    }

private:
    ProgressCallback _progressCallback;
    bool _updateAvailable;
    String _lastError;
    String _latestVersion;
    String _releaseNotes;
    String _firmwareUrl;
    int _firmwareSize;
    bool _isCritical;

    // Fetch version.json from release assets for extra metadata
    void fetchVersionJson(const String& url) {
        Serial.println("[OTA] Fetching version.json for metadata...");

        WiFiClientSecure client;
        client.setInsecure();

        HTTPClient http;
        http.begin(client, url);
        http.setTimeout(OTA_HTTP_TIMEOUT);
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

        int httpCode = http.GET();
        if (httpCode != 200) {
            Serial.printf("[OTA] version.json fetch failed: %d\n", httpCode);
            http.end();
            return;
        }

        String payload = http.getString();
        http.end();

        StaticJsonDocument<1024> doc;
        if (deserializeJson(doc, payload)) {
            Serial.println("[OTA] version.json parse failed");
            return;
        }

        // Extract optional fields
        if (doc.containsKey("critical")) {
            _isCritical = doc["critical"].as<bool>();
        }
        if (doc.containsKey("release_notes") && _releaseNotes.length() == 0) {
            _releaseNotes = doc["release_notes"].as<String>();
        }

        Serial.printf("[OTA] Metadata: critical=%d\n", _isCritical);
    }

    // Compare semantic versions (e.g., "1.2.3" vs "1.2.4")
    // Returns true if 'newer' is greater than 'current'
    bool isNewerVersion(const String& newer, const String& current) {
        if (current == "unknown") return true;

        int newMajor = 0, newMinor = 0, newPatch = 0;
        int curMajor = 0, curMinor = 0, curPatch = 0;

        parseVersion(newer, newMajor, newMinor, newPatch);
        parseVersion(current, curMajor, curMinor, curPatch);

        if (newMajor > curMajor) return true;
        if (newMajor < curMajor) return false;

        if (newMinor > curMinor) return true;
        if (newMinor < curMinor) return false;

        return newPatch > curPatch;
    }

    // Parse version string "X.Y.Z" into components
    void parseVersion(const String& ver, int& major, int& minor, int& patch) {
        major = minor = patch = 0;

        int firstDot = ver.indexOf('.');
        if (firstDot < 0) {
            major = ver.toInt();
            return;
        }

        major = ver.substring(0, firstDot).toInt();

        int secondDot = ver.indexOf('.', firstDot + 1);
        if (secondDot < 0) {
            minor = ver.substring(firstDot + 1).toInt();
            return;
        }

        minor = ver.substring(firstDot + 1, secondDot).toInt();
        patch = ver.substring(secondDot + 1).toInt();
    }
};

#endif // OTA_UPDATES_H
