#include "APP/app_wifi.h"
#include "APP/app_device_info.h"
#include "APP/app_ble.h"
#include "APP/app_input.h"
#include "APP/app_gallery.h"
#include "APP/app_power_manager.h"
#include "hal/hal.h"
#include "hal/hal_indicator.h"
#include "app_config.h"

#include "cJSON.h"
#include "ArduinoLog.h"

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <atomic>
#include <vector>
#include <algorithm>

#include "ESP32Ping.h"
#include "minirt.h"
#include "APP/app_events.h"

ESP_EVENT_DEFINE_BASE(WIFI_APP_EVENT_BASE);

struct FoundNetwork
{
    String ssid;
    int32_t channel;
    int8_t rssi;
    uint8_t *bssid;
};

struct ConnectionCandidate
{
    String ssid;
    String password;
    int32_t channel;
    uint8_t bssid[6];
    int8_t rssi;
};

static WebServer server(80);
static DNSServer dnsServer;
static bool portal_running = false;
static std::atomic<bool> g_provisioning_active{false};
static std::atomic<ProvisioningOrigin> g_provisioning_origin{ProvisioningOrigin::Unknown};
static String ap_ssid = "";
static bool is_portal_manual = false;
static volatile bool g_portal_hold_on_connect = false;

static EventGroupHandle_t wifi_event_group;
const int WIFI_EVENT_BIT_START_AUTOCONNECT = BIT0;
const int WIFI_EVENT_BIT_STA_CONNECTED = BIT1;
const int WIFI_EVENT_BIT_STA_DISCONNECTED = BIT2;
const int WIFI_EVENT_BIT_START_PORTAL = BIT3;
const int WIFI_EVENT_BIT_PORTAL_DONE = BIT4;
const int WIFI_EVENT_BIT_DO_CONNECT = BIT5;
const int WIFI_EVENT_BIT_NETWORK_CHECK = BIT6;
const int WIFI_EVENT_BIT_SCAN_REQUEST = BIT7;

static volatile uint8_t lastDisconnectReason = 0;
static volatile bool is_testing_credentials = false;
static volatile bool g_manual_autoconnect_requested = false;

struct app_wifi_autoconnect_event_data_t
{
    bool manual;
};

enum class AutoConnectResult : uint8_t
{
    Connected,
    Failed,
    NoCredentials,
    Portal,
};

static esp_timer_handle_t g_wifi_auto_timer;
static app_wifi_connect_args_t g_wifi_connect_args;
static SemaphoreHandle_t g_wifi_args_mutex;
static volatile bool g_internet_ok = false;
static esp_timer_handle_t g_periodic_check_timer;
static std::vector<CachedAPInfo, util::psram_allocator<CachedAPInfo>> g_wifi_scan_cache;
static std::atomic<bool> g_autoconnect_power_held{false};
bool _portal_connected = false;

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>WiFi Configuration</title>
    <style>
        body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; margin: 0; background-color: #f7f7f7; display: flex; justify-content: center; align-items: flex-start; min-height: 100vh; padding-top: 20px; box-sizing: border-box; }
        .container { background-color: white; padding: 25px; border-radius: 12px; box-shadow: 0 6px 20px rgba(0,0,0,0.08); width: 100%; max-width: 450px; box-sizing: border-box; margin: 0 15px; }
        h1 { color: #222; text-align: center; margin-bottom: 25px; font-weight: 600; }
        .network-list, .saved-list { list-style: none; padding: 0; margin: 0; border: 1px solid #e0e0e0; border-radius: 8px; overflow: hidden; }
        .network-list { max-height: 280px; overflow-y: auto; }
        .network-list li, .saved-list li { padding: 12px 15px; border-bottom: 1px solid #e9e9e9; display: flex; justify-content: space-between; align-items: center; transition: background-color 0.2s; }
        .network-list li { cursor: pointer; }
        .network-list li:last-child, .saved-list li:last-child { border-bottom: none; }
        .network-list li:hover { background-color: #f5f5f5; }
        .ssid { font-weight: 500; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; flex-grow: 1; }
        .rssi { color: #666; font-size: 0.9em; margin-left: 10px; }
        .loader, .message { text-align: center; padding: 20px; color: #555; }
        form { display: none; margin-top: 20px; }
        input[type="password"], input[type="text"] { width: 100%; padding: 12px 60px 12px 12px; margin-bottom: 10px; border: 1px solid #ccc; border-radius: 8px; box-sizing: border-box; font-size: 16px; }
        button, .connect-btn, .ignore-btn { width: 100%; padding: 12px; background: linear-gradient(to right, #007bff, #0056b3); color: white; border: none; border-radius: 8px; cursor: pointer; font-size: 16px; font-weight: 500; transition: opacity 0.2s; }
        button:hover, .connect-btn:hover, .ignore-btn:hover { opacity: 0.85; }
        .section-title { font-size: 1.1em; color: #333; margin-top: 25px; margin-bottom: 10px; font-weight: 500; display: flex; justify-content: space-between; align-items: center; }
        #refresh-btn { background: #6c757d; padding: 6px 12px; font-size: 12px; width: auto; }
        .button-group { display: flex; gap: 8px; }
        .connect-btn, .ignore-btn {
            width: auto;
            min-width: 80px;
            padding: 8px 14px;
            font-size: 14px;
            margin-left: 0;
            flex-shrink: 0;
        }
        .ignore-btn { background: #6c757d; }
        
        .password-wrapper { 
            position: relative; 
            width: 100%; 
            margin-bottom: 10px; 
        }
        /* --- Custom confirmation dialog style --- */
        .confirm-modal {
            position: fixed;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            background-color: rgba(0, 0, 0, 0.5);
            display: none;
            justify-content: center;
            align-items: center;
            z-index: 1000;
        }
        .confirm-dialog {
            background-color: white;
            padding: 25px;
            border-radius: 12px;
            box-shadow: 0 4px 15px rgba(0,0,0,0.2);
            width: 90%;
            max-width: 320px;
            text-align: center;
        }
        .confirm-dialog p {
            margin: 0 0 20px;
            font-size: 16px;
            color: #333;
            word-wrap: break-word;
        }
        .confirm-buttons {
            display: flex;
            justify-content: space-between;
            gap: 10px;
        }
        .confirm-buttons button {
            width: 100%;
            padding: 10px;
            font-size: 16px;
            border-radius: 8px;
            border: none;
            cursor: pointer;
        }
        #confirm-ok {
            background: #dc3545;
            color: white;
        }
        #confirm-cancel {
            background: #6c757d;
            color: white;
        }
        .toggle-password { 
            position: absolute; 
            top: 50%; 
            right: 15px; 
            transform: translateY(-50%); 
            cursor: pointer; 
            color: #007bff; 
            font-weight: 500; 
            font-size: 14px; 
            user-select: none;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Configure Wi-Fi</h1>
        <div id="status-message" class="message" style="display: none;"></div>
        <div id="saved-networks-section" style="display:none;">
            <div class="section-title">Saved Networks</div>
            <ul id="saved-list" class="saved-list"></ul>
        </div>
        <div class="section-title"><span>Available Networks</span><button id="refresh-btn">Refresh</button></div>
        <div id="loader" class="loader">Scanning...</div>
        <ul id="network-list" class="network-list"></ul>
        <form id="password-form">
            <h3 id="form-ssid" style="text-align:center; margin-top:0; margin-bottom: 15px; font-weight: 500;"></h3>
            <div class="password-wrapper">
                <input type="password" id="password" placeholder="Password">
                <span class="toggle-password">Show</span>
            </div>
            <button type="submit">Connect and Save</button>
        </form>
    </div>

    <div id="custom-confirm" class="confirm-modal">
        <div class="confirm-dialog">
            <p id="confirm-message"></p>
            <div class="confirm-buttons">
                <button id="confirm-cancel">Cancel</button>
                <button id="confirm-ok">Ignore</button>
            </div>
        </div>
    </div>

    <script>
        document.addEventListener('DOMContentLoaded', () => {
            const togglePassword = document.querySelector('.toggle-password');
            if (togglePassword) {
                togglePassword.addEventListener('click', function () {
                    // Check the current type of the input box
                    const type = passwordInput.getAttribute('type') === 'password' ? 'text' : 'password';
                    passwordInput.setAttribute('type', type);
                    // Change the text on the button
                    this.textContent = type === 'password' ? 'Show' : 'Hide';
                });
            }
            const WIFI_AUTH_OPEN = 0;

            const networkList = document.getElementById('network-list');
            const savedList = document.getElementById('saved-list');
            const savedSection = document.getElementById('saved-networks-section');
            const loader = document.getElementById('loader');
            const form = document.getElementById('password-form');
            const formSsid = document.getElementById('form-ssid');
            const passwordInput = document.getElementById('password');
            const refreshBtn = document.getElementById('refresh-btn');
            const statusMessage = document.getElementById('status-message');
            
            // --- Custom popup-related elements and logic ---
            const confirmModal = document.getElementById('custom-confirm');
            const confirmMessage = document.getElementById('confirm-message');
            const confirmOkBtn = document.getElementById('confirm-ok');
            const confirmCancelBtn = document.getElementById('confirm-cancel');

            // Create a reusable Promise to handle confirmation logic
            const showCustomConfirm = (message) => {
                return new Promise((resolve) => {
                    confirmMessage.textContent = message;
                    confirmModal.style.display = 'flex';

                    confirmOkBtn.onclick = () => {
                        confirmModal.style.display = 'none';
                        resolve(true);
                    };
                    confirmCancelBtn.onclick = () => {
                        confirmModal.style.display = 'none';
                        resolve(false);
                    };
                });
            };

            const showMessage = (msg, isError = false) => {
                statusMessage.textContent = msg;
                statusMessage.style.color = isError ? '#d93025' : '#1a73e8';
                statusMessage.style.display = 'block';
                statusMessage.style.padding = '10px';
                statusMessage.style.backgroundColor = isError ? '#fce8e6' : '#e8f0fe';
                statusMessage.style.borderRadius = '8px';
                statusMessage.style.border = isError ? '1px solid #d93025' : '1px solid #1a73e8';
            };
            const submitConnect = async (ssid, password, useSavedCredentials = false) => {
                showMessage(`Connecting to ${ssid}...`);

                try {
                    const response = await fetch('/submit', {
                        method: 'POST',
                        headers: {'Content-Type': 'application/json'},
                        body: JSON.stringify({ ssid, password, useSavedCredentials })
                    });

                    const result = await response.json();
                    if (result.success) {
                        showMessage('Connection successful! The device will now connect.');
                        setTimeout(() => window.location.href = '/done.html', 2000);
                    } else {
                        showMessage(`Failed to connect: ${result.error}`, true);
                    }
                } catch (err) {
                    showMessage('Failed to connect: Network error.', true);
                }
            };

            const fetchNetworks = async () => {
                loader.style.display = 'block';
                networkList.innerHTML = '';
                form.style.display = 'none';
                try {
                    const response = await fetch('/scan'); 
                    const networks = await response.json();
                    networks.sort((a, b) => b.rssi - a.rssi);
                    if (networks.length === 0) {
                       networkList.innerHTML = '<li style="justify-content:center;">No networks found.</li>';
                    } else {
                        networks.forEach(net => {
                            const li = document.createElement('li');
                            const securityLabel = net.authmode === WIFI_AUTH_OPEN ? 'Open' : 'Secured';
                            li.innerHTML = `<span class="ssid">${net.ssid}</span> <span class="rssi">${net.rssi} dBm · ${securityLabel}</span>`;
                            li.dataset.authmode = net.authmode;
                            li.addEventListener('click', () => {
                                form.dataset.ssid = net.ssid;
                                form.dataset.authmode = net.authmode;
                                passwordInput.value = '';

                                if (net.authmode === WIFI_AUTH_OPEN) {
                                    form.style.display = 'none';
                                    passwordInput.required = false;
                                    submitConnect(net.ssid, '', false);
                                } else {
                                    formSsid.textContent = `Enter password for: ${net.ssid}`;
                                    form.style.display = 'block';
                                    passwordInput.required = true;
                                    passwordInput.focus();
                                }
                            });
                            networkList.appendChild(li);
                        });
                    }
                } catch (e) {
                    networkList.innerHTML = '<li>Scan failed. Please refresh.</li>';
                } finally {
                    loader.style.display = 'none';
                }
            };
            const fetchSavedNetworks = async () => {
                try {
                    const response = await fetch('/saved');
                    const networks = await response.json();
                    if (networks.length > 0) {
                        savedSection.style.display = 'block';
                        savedList.innerHTML = '';
                        networks.forEach(net => {
                            const li = document.createElement('li');
                            li.innerHTML = `<span class="ssid">${net.ssid}</span>
                                          <div class="button-group">
                                              <button class="connect-btn" data-ssid="${net.ssid}">Connect</button>
                                              <button class="ignore-btn" data-ssid="${net.ssid}">Ignore</button>
                                          </div>`;
                            savedList.appendChild(li);
                        });
                    } else {
                        savedSection.style.display = 'none';
                    }
                } catch (e) { console.error('Could not fetch saved networks.'); }
            };

            document.getElementById('saved-list').addEventListener('click', async (e) => {
                const target = e.target;

                if (target.classList.contains('connect-btn')) {
                    e.stopPropagation();
                    const ssid = target.dataset.ssid;
                    await submitConnect(ssid, '', true);
                } 
                // --- “Ignore” button ---
                else if (target.classList.contains('ignore-btn')) {
                    e.stopPropagation();
                    const ssid = target.dataset.ssid;
                    
                    const confirmed = await showCustomConfirm(`Are you sure you want to ignore "${ssid}"?`);

                    if (!confirmed) {
                        return;
                    }

                    // If the user clicks confirm, execute the logic below
                    showMessage(`Ignoring ${ssid}...`);
                    const response = await fetch('/ignore', {
                        method: 'POST',
                        headers: {'Content-Type': 'application/json'},
                        body: JSON.stringify({ ssid: ssid })
                    });
                    const result = await response.json();
                    if (result.success) {
                        showMessage(`Network ${ssid} has been ignored.`);
                        const listItem = target.closest('li');
                        listItem.remove();
                        if (savedList.children.length === 0) {
                            savedSection.style.display = 'none';
                        }
                    } else {
                        showMessage(`Failed to ignore network: ${result.error || 'Server error'}`, true);
                    }
                }
            });
            form.addEventListener('submit', async (e) => {
                e.preventDefault();
                const ssid = form.dataset.ssid;
                const authmode = parseInt(form.dataset.authmode || '-1', 10);
                const requiresPassword = authmode !== WIFI_AUTH_OPEN;
                const password = passwordInput.value;

                if (requiresPassword && password.trim() === '') {
                    showMessage('Password is required for this network.', true);
                    return;
                }

                await submitConnect(ssid, password, false);
            });
            // refreshBtn.addEventListener('click', fetchNetworks);
            refreshBtn.addEventListener('click', async () => {
                loader.style.display = 'block';
                networkList.innerHTML = '';
                try {
                    await fetch('/refresh_scan');
                    await new Promise(resolve => setTimeout(resolve, 4000));
                    fetchNetworks();

                } catch (e) {
                    loader.style.display = 'none';
                    networkList.innerHTML = '<li>Scan failed. Please refresh.</li>';
                }
            });

            fetchSavedNetworks();
            fetchNetworks();
        });
    </script>
</body>
</html>
)rawliteral";

const char DONE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Configuration Complete</title>
    <style>
        body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; margin: 0; background-color: #f4f4f4; display: flex; justify-content: center; align-items: center; text-align: center; min-height: 100vh; }
        .container { background-color: white; padding: 40px; border-radius: 12px; box-shadow: 0 6px 20px rgba(0,0,0,0.08); }
        h1 { color: #28a745; font-weight: 600; }
        p { color: #333; }
    </style>
</head>
<body>
    <div class="container">
        <h1>✓ Success!</h1>
        <p>Wi-Fi is configured. The device is connecting to the selected network.</p>
        <p>You can close this page.</p>
    </div>
</body>
</html>
)rawliteral";

static void handleRoot();
static void handleRefreshScan();
static void handleScan();
static void handleGetSaved();
static void handleSubmit();
static void handleIgnore();
static void handleDone();
static void handleNotFound();
static void handleCaptiveApple();
static void handleCaptiveAndroid();
static void handleCaptiveWindows();
static bool tryToConnect(String ssid, String password, bool saveOnSuccess, bool andDisconnect = true);
static int findBestChannel();
static AutoConnectResult attemptAutoConnect(bool manual_request = false);
static void startPortal();
static void stopPortal();

static void release_autoconnect_power()
{
    if (!g_autoconnect_power_held.exchange(false))
    {
        return;
    }

    app_power_manager_release(AppPowerOwner::WifiConnect);
}

bool get_portal_status()
{
    return portal_running;
}

static void set_provisioning_active(bool active)
{
    g_provisioning_active.store(active);
}

bool app_wifi_is_provisioning()
{
    return g_provisioning_active.load();
}

ProvisioningOrigin app_wifi_get_origin()
{
    return g_provisioning_origin.load();
}

void app_wifi_clear_origin()
{
    g_provisioning_origin.store(ProvisioningOrigin::Unknown);
}

static void handleRoot()
{
    app_power_manager_activity(AppPowerOwner::UserAction);
    server.send_P(200, "text/html", INDEX_HTML);
}

static void handleDone()
{
    app_power_manager_activity(AppPowerOwner::UserAction);
    server.send_P(200, "text/html", DONE_HTML);
}

static void handleNotFound()
{
    app_power_manager_activity(AppPowerOwner::UserAction);
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
}

static void handleCaptiveApple()
{
    app_power_manager_activity(AppPowerOwner::UserAction);
    server.send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
}

static void handleCaptiveAndroid()
{
    app_power_manager_activity(AppPowerOwner::UserAction);
    server.send(204, "text/plain", "");
}

static void handleCaptiveWindows()
{
    app_power_manager_activity(AppPowerOwner::UserAction);
    server.send(200, "text/plain", "Microsoft NCSI");
}

static void handleRefreshScan()
{
    app_power_manager_activity(AppPowerOwner::UserAction);
    Log.infoln("[app_wifi] Refresh scan requested from web.");
    esp_event_post(WIFI_APP_EVENT_BASE, APP_WIFI_EVENT_SCAN, NULL, 0, portMAX_DELAY);
    server.send(200, "application/json", "{\"success\":true, \"message\":\"Scan initiated\"}");
}

static void handleScan()
{
    app_power_manager_activity(AppPowerOwner::UserAction);
    Log.verboseln("[app_wifi] handleScan: Reading from cache. Cache size: %d", g_wifi_scan_cache.size());
    cJSON *root = cJSON_CreateArray();
    if (root == NULL)
    {
        server.send(500, "application/json", "{\"success\":false,\"error\":\"Memory allocation failed\"}");
        return;
    }

    for (const auto &net : g_wifi_scan_cache)
    {
        cJSON *net_json = cJSON_CreateObject();
        if (net_json == NULL)
            continue;

        cJSON_AddStringToObject(net_json, "ssid", net.ssid);
        cJSON_AddNumberToObject(net_json, "rssi", net.rssi);
        cJSON_AddNumberToObject(net_json, "authmode", net.authmode);
        cJSON_AddItemToArray(root, net_json);
    }

    char *json_output = cJSON_Print(root);
    cJSON_Delete(root);

    server.send(200, "application/json", json_output);
    free(json_output);
}

static void handleGetSaved()
{
    app_power_manager_activity(AppPowerOwner::UserAction);
    std::vector<std::pair<String, String>> credentials;
    GetWifiCredentials(credentials);

    cJSON *root = cJSON_CreateArray();
    if (root == NULL)
    {
        server.send(500, "application/json", "{\"success\":false,\"error\":\"Memory allocation failed\"}");
        return;
    }

    for (const auto &cred : credentials)
    {
        cJSON *obj = cJSON_CreateObject();
        if (obj == NULL)
            continue;
        cJSON_AddStringToObject(obj, "ssid", cred.first.c_str());
        cJSON_AddItemToArray(root, obj);
    }

    char *json_output = cJSON_Print(root);
    cJSON_Delete(root);

    server.send(200, "application/json", json_output);
    free(json_output);
}

static void handleSubmit()
{
    app_power_manager_activity(AppPowerOwner::UserAction);
    cJSON *root = cJSON_Parse(server.arg("plain").c_str());

    if (root == NULL)
    {
        Log.errorln("[app_wifi] handleSubmit cJSON_Parse() failed");
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid request format\"}");
        return;
    }

    cJSON *ssid_item = cJSON_GetObjectItemCaseSensitive(root, "ssid");
    cJSON *password_item = cJSON_GetObjectItemCaseSensitive(root, "password");
    cJSON *use_saved_item = cJSON_GetObjectItemCaseSensitive(root, "useSavedCredentials");

    String ssid = (cJSON_IsString(ssid_item) && (ssid_item->valuestring != NULL)) ? ssid_item->valuestring : "";
    String password = (cJSON_IsString(password_item) && (password_item->valuestring != NULL)) ? password_item->valuestring : "";

    bool has_use_saved_flag = cJSON_IsBool(use_saved_item);
    bool use_saved_credentials = has_use_saved_flag ? cJSON_IsTrue(use_saved_item) : password.isEmpty();

    cJSON_Delete(root);

    if (ssid.isEmpty())
    {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid SSID\"}");
        return;
    }

    if (use_saved_credentials)
    {
        if (!GetWifiPassword(ssid, password))
        {
            server.send(200, "application/json", "{\"success\":false,\"error\":\"Could not find password for saved network. Please re-enter.\"}");
            return;
        }
    }

    g_portal_hold_on_connect = true;

    if (tryToConnect(ssid, password, true, false))
    {
        Log.infoln("[app_wifi] Credentials verified. Responding to client.");
        server.send(200, "application/json", "{\"success\":true}");

        delay(200);
        esp_event_post(WIFI_APP_EVENT_BASE, APP_WIFI_EVENT_PORTAL_DONE, NULL, 0, portMAX_DELAY);
    }
    else
    {
        g_portal_hold_on_connect = false;
        uint8_t reason = lastDisconnectReason;
        String shortMsg;

        switch (reason)
        {
        case 8:
            shortMsg = "assoc leave";
            break;
        case 201:
            shortMsg = "no AP found";
            break;
        case 202:
            shortMsg = "authentication failed";
            break;
        case 203:
            shortMsg = "association failed";
            break;
        case 15:
        case 204:
            shortMsg = "handshake timeout";
            break;
        case 39: // TIMEOUT
            shortMsg = "timeout";
            break;
        default:
            shortMsg = String(reason);
            break;
        }

        String payload = String("{\"success\":false,"
                                "\"error\":\"") +
                         shortMsg + "\"}";
        server.send(200, "application/json", payload);
    }
}

static void handleIgnore()
{
    app_power_manager_activity(AppPowerOwner::UserAction);
    cJSON *root = cJSON_Parse(server.arg("plain").c_str());

    if (root == NULL)
    {
        Log.errorln("[app_wifi] handleIgnore cJSON_Parse() failed");
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid request format\"}");
        return;
    }

    cJSON *ssid_item = cJSON_GetObjectItemCaseSensitive(root, "ssid");
    String ssid = (cJSON_IsString(ssid_item) && (ssid_item->valuestring != NULL)) ? ssid_item->valuestring : "";

    cJSON_Delete(root);

    if (ssid.isEmpty())
    {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid SSID\"}");
        return;
    }

    Log.infoln("[app_wifi] Request to ignore network: %s", ssid.c_str());
    if (RemoveWifiCredential(ssid))
    {
        Log.infoln("[app_wifi] Successfully removed '%s' from NVS.", ssid.c_str());
        server.send(200, "application/json", "{\"success\":true}");
    }
    else
    {
        Log.errorln("[app_wifi] Failed to remove '%s' from NVS.", ssid.c_str());
        server.send(500, "application/json", "{\"success\":false,\"error\":\"Failed to update storage\"}");
    }
}

static String bssid_to_string(const uint8_t *bssid)
{
    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
    return String(mac_str);
}

static bool tryToConnect(String ssid, String password, bool saveOnSuccess, bool andDisconnect)
{
    is_testing_credentials = true;
    Log.infoln("[app_wifi] Testing credentials for: %s", ssid.c_str());

    if (g_wifi_scan_cache.empty())
    {
        Log.errorln("[app_wifi] Scan cache is empty. Cannot test connection. Please scan first.");
        is_testing_credentials = false;
        return false;
    }

    std::vector<CachedAPInfo, util::psram_allocator<CachedAPInfo>> found_aps;
    for (const auto &net : g_wifi_scan_cache)
    {
        if (strcmp(net.ssid, ssid.c_str()) == 0)
        {
            found_aps.push_back(net);
        }
    }

    if (found_aps.empty())
    {
        Log.errorln("[app_wifi] Specified SSID '%s' not found in scan results.", ssid.c_str());
        is_testing_credentials = false;
        return false;
    }

    std::sort(found_aps.begin(), found_aps.end(), [](const CachedAPInfo &a, const CachedAPInfo &b)
              { return a.rssi > b.rssi; });

    CachedAPInfo best_ap = found_aps[0];

    for (int attempt = 1; attempt <= 3; attempt++)
    {
        Log.infoln("[app_wifi] ==> Attempt #%d of 3: Connecting to SSID: %s (BSSID: %s, RSSI: %d)",
                   attempt,
                   ssid.c_str(),
                   bssid_to_string(best_ap.bssid).c_str(),
                   best_ap.rssi);

        lastDisconnectReason = 0;
        const char *pass_ptr = password.length() > 0 ? password.c_str() : nullptr;
        WiFi.begin(ssid.c_str(), pass_ptr, best_ap.channel, best_ap.bssid);

        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED)
        {
            if (lastDisconnectReason != 0)
            {
                Log.errorln("[app_wifi] Early disconnect from BSSID %s, reason=%d", bssid_to_string(best_ap.bssid).c_str(), lastDisconnectReason);
                break;
            }
            delay(500);
            if (millis() - start > 10000)
            {
                Log.errorln("[app_wifi] Connection attempt #%d timed out.", attempt);
                lastDisconnectReason = 0xFF;
                break;
            }
        }

        if (WiFi.status() == WL_CONNECTED)
        {
            Serial.println();
            Log.verboseln("[app_wifi] Credentials are valid. Connection successful to BSSID %s!", bssid_to_string(best_ap.bssid).c_str());
            Log.infoln("[app_wifi] IP Address was: %s", WiFi.localIP().toString().c_str());

            if (saveOnSuccess)
            {
                SaveWifiCredential(ssid, password);
                Log.infoln("[app_wifi] Credentials saved successfully.");
            }

            if (andDisconnect)
            {
                WiFi.disconnect(true, false);
                delay(100);
            }

            is_testing_credentials = false;
            return true;
        }
        else
        {
            Log.warningln("[app_wifi] Attempt #%d failed.", attempt);
            WiFi.disconnect(true, false);
            delay(100);

            if (attempt < 3)
            {
                Log.infoln("Retrying in 2 seconds...");
                delay(2000);
            }
        }
    }

    Log.errorln("[app_wifi] All 3 connection attempts for SSID '%s' failed.", ssid.c_str());
    is_testing_credentials = false;
    return false;
}

static bool has_cached_image()
{
    return HasImage() && GetImageCount() > 0 && app_gallery_has_playable();
}

static AutoConnectResult start_portal_from_autoconnect()
{
    set_provisioning_active(true);
    esp_event_post(VIEW_EVENT_BASE, VIEW_EVENT_SHOW_STARTUP, NULL, 1, portMAX_DELAY);
    esp_event_post(WIFI_APP_EVENT_BASE, APP_WIFI_EVENT_START_PORTAL, NULL, 0, portMAX_DELAY);
    return AutoConnectResult::Portal;
}

static AutoConnectResult attemptAutoConnect(bool manual_request)
{
    std::vector<std::pair<String, String>> credentials;
    const bool force_portal = HAL::GetHAL().wakeupReason() == WakeupClick::DoubleLong;
    if (!GetWifiCredentials(credentials) || credentials.empty())
    {
        if (force_portal || manual_request || !has_cached_image())
        {
            return start_portal_from_autoconnect();
        }
        else
        {
            Log.infoln("[app_wifi] No saved WiFi credentials; using cached image.");
        }
        return AutoConnectResult::NoCredentials;
    }

    if (force_portal)
    {
        return start_portal_from_autoconnect();
    }

    WiFi.mode(WIFI_STA);
    Log.infoln("[app_wifi] Starting auto-connect the saved Wi-Fi networks...");

    Log.infoln("[app_wifi] Found %d saved networks. Will try them in order.", credentials.size());

    for (const auto &candidate : credentials)
    {
        if (xEventGroupGetBits(wifi_event_group) & WIFI_EVENT_BIT_START_PORTAL)
        {
            Log.infoln("[app_wifi] Manual portal start requested, aborting auto-connect.");
            return AutoConnectResult::Portal;
        }

        Log.infoln("[app_wifi] ===> Attempting to connect to SSID: '%s'", candidate.first.c_str());

        if (candidate.second.length() == 0)
        {
            WiFi.begin(candidate.first.c_str());
        }
        else
        {
            WiFi.begin(candidate.first.c_str(), candidate.second.c_str());
        }

        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED)
        {
            if (xEventGroupGetBits(wifi_event_group) & WIFI_EVENT_BIT_START_PORTAL)
            {
                Log.verboseln("[app_wifi] Manual portal start requested during connection attempt, aborting.");
                WiFi.disconnect(true, true);
                return AutoConnectResult::Portal;
            }

            if (millis() - start > 10000)
            {
                Log.errorln("[app_wifi] -> Connection to '%s' timed out.", candidate.first.c_str());
                break;
            }
            delay(500);
        }

        if (WiFi.status() == WL_CONNECTED)
        {
            Log.infoln("[app_wifi] Auto-connect successful! Connected to '%s'.", candidate.first.c_str());
            Log.infoln("[app_wifi] IP Address: %s", WiFi.localIP().toString().c_str());
            SaveWifiCredential(candidate.first, candidate.second);
            return AutoConnectResult::Connected;
        }
        else
        {
            Log.warningln("[app_wifi] -> Failed to connect to '%s'. Trying next saved network...", candidate.first.c_str());
            WiFi.disconnect(true, true);
            delay(100);
        }
    }

    Log.errorln("[app_wifi] Auto-connect failed. All attempts for all visible saved networks have failed.");

    if (!has_cached_image())
    {
        return start_portal_from_autoconnect();
    }

    return AutoConnectResult::Failed;
}

static int findBestChannel()
{
    Log.infoln("[app_wifi] Scanning for best AP channel...");
    int channelCounts[14] = {0};
    int n = WiFi.scanNetworks(false, true);

    if (n == 0)
    {
        Log.infoln("[app_wifi] No networks found, defaulting to channel 1.");
        return 1;
    }

    for (int i = 0; i < n; ++i)
    {
        int channel = WiFi.channel(i);
        if (channel >= 1 && channel <= 13)
        {
            channelCounts[channel]++;
        }
    }

    int bestChannel = 1;
    int minCount = channelCounts[1];
    // find the best channel from 1 to 13
    for (int ch = 1; ch <= 13; ++ch)
    {
        if (channelCounts[ch] < minCount)
        {
            minCount = channelCounts[ch];
            bestChannel = ch;
        }
    }
    WiFi.scanDelete();

    Log.infoln("[app_wifi] Best channel found: %d", bestChannel);
    return bestChannel;
}

static void startPortal()
{
    if (portal_running)
    {
        release_autoconnect_power();
        return;
    }

    Log.infoln("[app_wifi] Starting WiFi Configuration Portal.");

    uint8_t mac[6];
    WiFi.macAddress(mac);
    char mac_suffix[5];
    sprintf(mac_suffix, "%02x%02x", mac[4], mac[5]);

    const char *ap_prefix = HAL::GetHAL().apPrefix();
    String ap_ssid = String(ap_prefix ? ap_prefix : "ePaper DIY Kit") + "-" + mac_suffix;

    Log.infoln("[app_wifi] AP SSID: %s", ap_ssid.c_str());

    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    delay(100);

    WiFi.mode(WIFI_AP_STA);
    int bestChannel = findBestChannel();
    WiFi.softAP(ap_ssid.c_str(), nullptr, bestChannel);

    delay(100);

    Log.infoln("[app_wifi] Triggering initial background scan for portal.");
    esp_event_post(WIFI_APP_EVENT_BASE, APP_WIFI_EVENT_SCAN, NULL, 0, portMAX_DELAY);
    dnsServer.start(53, "*", WiFi.softAPIP());
    Log.infoln("[app_wifi] AP IP address: %s", WiFi.softAPIP().toString().c_str());

    server.on("/", HTTP_GET, handleRoot);
    server.on("/refresh_scan", HTTP_GET, handleRefreshScan);
    server.on("/scan", HTTP_GET, handleScan);
    server.on("/saved", HTTP_GET, handleGetSaved);
    server.on("/submit", HTTP_POST, handleSubmit);
    server.on("/ignore", HTTP_POST, handleIgnore);
    server.on("/done.html", HTTP_GET, handleDone);

    server.on("/hotspot-detect.html", HTTP_GET, handleCaptiveApple);
    server.on("/generate_204", HTTP_GET, handleCaptiveAndroid);
    server.on("/ncsi.txt", HTTP_GET, handleCaptiveWindows);

    server.onNotFound(handleNotFound);

    server.begin();
    Log.infoln("[app_wifi] Web server started. Connect to the AP to configure.");
    portal_running = true;
    app_power_manager_acquire(AppPowerOwner::Provisioning);
    release_autoconnect_power();
}

static void stopPortal()
{
    if (!portal_running)
        return;

    server.stop();
    dnsServer.stop();
    WiFi.softAPdisconnect(true);
    portal_running = false;
    g_portal_hold_on_connect = false;
    app_power_manager_release(AppPowerOwner::Provisioning);
    Log.infoln("[app_wifi] Configuration portal stopped.");
}

static void stop_wifi_auto_timer()
{
    if (g_wifi_auto_timer != nullptr && esp_timer_is_active(g_wifi_auto_timer))
    {
        esp_timer_stop(g_wifi_auto_timer);
    }
}

static void start_manual_provisioning()
{
    app_power_manager_activity(AppPowerOwner::UserAction);
    Log.infoln("[app_wifi] Manual provisioning requested by input.");

    CancelPendingWakeupAction();
    stop_wifi_auto_timer();
    SetLocalGallerySession(false);
    StopDeepSleepTimer();
    hal_indicator_play(HAL_INDICATOR_CLICK);
    set_provisioning_active(true);
    is_portal_manual = true;

    esp_event_post(VIEW_EVENT_BASE, VIEW_EVENT_SHOW_STARTUP, NULL, 1, portMAX_DELAY);
    xEventGroupSetBits(wifi_event_group, WIFI_EVENT_BIT_START_PORTAL);
}

static void app_wifi_input_event_handler(void *, esp_event_base_t, int32_t event_id, void *)
{
    if (event_id == APP_INPUT_EVENT_ENTER_PROVISIONING)
    {
        start_manual_provisioning();
    }
}

static void app_wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_APP_EVENT_BASE && wifi_event_group != NULL)
    {
        switch (id)
        {
        case APP_WIFI_EVENT_START_AUTOCONNECT:
        {
            auto *event_data = static_cast<app_wifi_autoconnect_event_data_t *>(data);
            if (event_data && event_data->manual)
            {
                g_manual_autoconnect_requested = true;
            }
            xEventGroupSetBits(wifi_event_group, WIFI_EVENT_BIT_START_AUTOCONNECT);
            break;
        }
        case APP_WIFI_EVENT_START_PORTAL:
            xEventGroupSetBits(wifi_event_group, WIFI_EVENT_BIT_START_PORTAL);
            break;
        case APP_WIFI_EVENT_PORTAL_DONE:
            xEventGroupSetBits(wifi_event_group, WIFI_EVENT_BIT_PORTAL_DONE);
            break;

        case APP_WIFI_EVENT_CONNECT:
        {
            app_power_manager_activity(AppPowerOwner::Config);
            Log.infoln("[app_wifi] Received APP_WIFI_EVENT_CONNECT event.");
            app_wifi_connect_args_t *args = (app_wifi_connect_args_t *)data;
            _portal_connected = false;

            if (xSemaphoreTake(g_wifi_args_mutex, portMAX_DELAY) == pdTRUE)
            {
                strlcpy(g_wifi_connect_args.ssid, args->ssid, sizeof(g_wifi_connect_args.ssid));
                strlcpy(g_wifi_connect_args.password, args->password, sizeof(g_wifi_connect_args.password));
                g_wifi_connect_args.origin = args->origin;
                g_provisioning_origin.store(args->origin);

                xSemaphoreGive(g_wifi_args_mutex);

                xEventGroupSetBits(wifi_event_group, WIFI_EVENT_BIT_DO_CONNECT);
            }

            break;
        }

        case APP_MQTT_DISCONNECTED:
        {
            Log.infoln("[app_wifi] APP_MQTT_DISCONNECTED");

            xEventGroupSetBits(wifi_event_group, WIFI_EVENT_BIT_NETWORK_CHECK);
            esp_event_post(CTRL_EVENT_BASE, SENSECRAFT_EVENT_STOP, NULL, 1, portMAX_DELAY);

            break;
        }

        case APP_WIFI_EVENT_SCAN:
        {
            if (wifi_event_group != NULL)
            {
                xEventGroupSetBits(wifi_event_group, WIFI_EVENT_BIT_SCAN_REQUEST);
            }
            break;
        }

        case APP_WIFI_EVENT_SCAN_DONE:
        {
            Log.verboseln("[app_wifi] Received APP_WIFI_EVENT_SCAN_DONE event.");
            Log.verboseln("------------------- Scan Results -------------------");
            Log.verboseln("Found %d access points:", g_wifi_scan_cache.size());

            int count = 0;
            const int max_to_print = 10;
            for (const auto &record : g_wifi_scan_cache)
            {
                if (count++ >= max_to_print)
                    break;
                Log.verboseln("  - SSID: %s, RSSI: %d dBm, Channel: %d", record.ssid, record.rssi, record.channel);
            }
            Log.verboseln("----------------------------------------------");

            break;
        }

        default:
            break;
        }
    }
}

static void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info)
{
    switch (event)
    {
    case ARDUINO_EVENT_WIFI_STA_START:
        // Log.infoln("[WiFi Event] STA Started");
        break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        Log.infoln("[WiFi Event] STA Got IP: %s", WiFi.localIP().toString().c_str());
        if (wifi_event_group != NULL)
        {
            xEventGroupSetBits(wifi_event_group, WIFI_EVENT_BIT_STA_CONNECTED);
        }
        // app_ble_deinit();
        break;

    case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
        // Log.verboseln("A client has connected to the AP. De-initializing BLE...");
        break;

    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
        Log.verboseln("Client has disconnected from the AP.");
        break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        Log.errorln("[WiFi Event] STA Disconnected. Reason: %d", info.wifi_sta_disconnected.reason);
        lastDisconnectReason = info.wifi_sta_disconnected.reason;
        if (wifi_event_group != NULL)
        {
            xEventGroupSetBits(wifi_event_group, WIFI_EVENT_BIT_STA_DISCONNECTED);
        }
        break;

    case SYSTEM_EVENT_STA_CONNECTED:
        Log.infoln("[WiFi Event] Connected to SSID %s", WiFi.SSID().c_str());

        break;

    default:
        break;
    }
}

static void _timer_cb_wifi(void *arg)
{
    app_wifi_request_autoconnect(false);
}

void app_wifi_request_autoconnect(bool manual)
{
    if (manual)
    {
        app_power_manager_activity(AppPowerOwner::UserAction);
    }

    app_wifi_autoconnect_event_data_t event_data = {
        .manual = manual,
    };
    esp_err_t err = esp_event_post(
        WIFI_APP_EVENT_BASE,
        APP_WIFI_EVENT_START_AUTOCONNECT,
        &event_data,
        sizeof(event_data),
        portMAX_DELAY);
    if (err != ESP_OK)
    {
        Log.errorln("[app_wifi] Failed to post autoconnect event: err=%d", err);
        if (!manual)
        {
            release_autoconnect_power();
        }
    }
}

const std::vector<CachedAPInfo, util::psram_allocator<CachedAPInfo>> &app_wifi_get_scan_cache()
{
    return g_wifi_scan_cache;
}

namespace
{
#ifndef WIFI_SCAN_RUNNING
#define WIFI_SCAN_RUNNING (-1)
#endif
#ifndef WIFI_SCAN_FAILED
#define WIFI_SCAN_FAILED (-2)
#endif
    constexpr uint32_t WIFI_RETRY_BACKOFF_MS = 60000;

    class WifiApp final
    {
    public:
        const char *name() const { return "WifiApp"; }
        void start(uint32_t now_ms)
        {
            _last_portal_service_ms = now_ms;
        }

        void run()
        {
            if (!_started)
            {
                Log.infoln("[app_wifi] WiFi manager app started");
                _started = true;
            }

            const uint32_t now_ms = millis();
            const bool local_gallery = IsLocalGallerySession();
            process_event_bits(now_ms, local_gallery);
            if (local_gallery)
            {
                handle_pending_portal_done(now_ms);
                handle_network_check(now_ms);
                handle_scan_progress(now_ms);
                service_portal(now_ms);
                return;
            }
            handle_pending_reconnect(now_ms);
            handle_pending_portal_done(now_ms);
            handle_network_check(now_ms);
            handle_scan_progress(now_ms);
            service_portal(now_ms);
        }

    private:
        enum class ScanState : uint8_t
        {
            Idle,
            Running
        };

        bool take_manual_autoconnect_request()
        {
            bool requested = g_manual_autoconnect_requested;
            if (requested)
            {
                g_manual_autoconnect_requested = false;
            }
            return requested;
        }

        bool should_keep_online() const
        {
            return !GetDeepSleepEnabled() || HAL::GetHAL().pmicIsCharging();
        }

        void process_event_bits(uint32_t now_ms, bool local_gallery)
        {
            if (!wifi_event_group)
            {
                return;
            }

            EventBits_t bits = xEventGroupWaitBits(
                wifi_event_group,
                WIFI_EVENT_BIT_START_AUTOCONNECT |
                    WIFI_EVENT_BIT_STA_CONNECTED |
                    WIFI_EVENT_BIT_STA_DISCONNECTED |
                    WIFI_EVENT_BIT_START_PORTAL |
                    WIFI_EVENT_BIT_PORTAL_DONE |
                    WIFI_EVENT_BIT_DO_CONNECT |
                    WIFI_EVENT_BIT_NETWORK_CHECK |
                    WIFI_EVENT_BIT_SCAN_REQUEST,
                pdTRUE,
                pdFALSE,
                0);

            if (bits == 0)
            {
                return;
            }

            if (bits & WIFI_EVENT_BIT_START_PORTAL)
            {
                Log.infoln("[app_wifi] Event: Start Portal");
                g_provisioning_origin.store(ProvisioningOrigin::WebPortal);
                _is_stably_connected = false;
                _portal_connected = true;

                stopPortal();

                set_provisioning_active(true);
                WiFi.disconnect(true, true);
                WiFi.mode(WIFI_OFF);
                delay(100);
                startPortal();

                app_ble_start_advertising();
            }

            if (bits & WIFI_EVENT_BIT_PORTAL_DONE)
            {
                Log.infoln("[app_wifi] Event: Portal Done");
                _pending_portal_done = true;
                _portal_done_due_ms = now_ms + 5000;
            }

            if (bits & WIFI_EVENT_BIT_START_AUTOCONNECT)
            {
                const bool manual_request = take_manual_autoconnect_request();
                handle_autoconnect(manual_request);
            }

            if (bits & WIFI_EVENT_BIT_STA_DISCONNECTED)
            {
                Log.infoln("[app_wifi] Event: WiFi Disconnected");
                esp_event_post(CTRL_EVENT_BASE, SENSECRAFT_EVENT_STOP, NULL, 1, portMAX_DELAY);

                if (_is_stably_connected)
                {
                    _is_stably_connected = false;
                    if (local_gallery)
                    {
                        Log.infoln("[app_wifi] Gallery mode: automatic reconnect deferred until manual request.");
                        _pending_autoconnect = false;
                    }
                    else if (should_keep_online())
                    {
                        Log.infoln("[app_wifi] Reconnecting in 5s...");
                        _pending_autoconnect = true;
                        _reconnect_due_ms = now_ms + 5000;
                    }
                    else
                    {
                        Log.infoln("[app_wifi] Background reconnect skipped; device may enter deep sleep.");
                        _pending_autoconnect = false;
                    }
                }
                else
                {
                    Log.infoln("Ignoring spurious disconnect event.");
                }
            }

            if (bits & WIFI_EVENT_BIT_STA_CONNECTED)
            {
                Log.infoln("[app_wifi] Event: WiFi Connected");
                _is_stably_connected = true;
                _pending_autoconnect = false;

                if (!g_portal_hold_on_connect)
                {
                    stopPortal();
                }
                else
                {
                    Log.verboseln("[app_wifi] Portal stop deferred to allow client acknowledgement.");
                }

                if (is_portal_manual)
                {
                    if (GetBindState())
                    {
                        esp_event_post(VIEW_EVENT_BASE, VIEW_EVENT_SHOW_IMAGE, NULL, 1, portMAX_DELAY);
                    }
                    is_portal_manual = false;
                }
                set_provisioning_active(false);

                if(!_portal_connected)
                {
                    if (_is_first_connected)
                    {
                        _is_first_connected = false;
                        esp_event_post(CTRL_EVENT_BASE, SENSECRAFT_EVENT_START, NULL, 1, portMAX_DELAY);
                    }
                    else
                    {
                        esp_event_post(CTRL_EVENT_BASE, SENSECRAFT_EVENT_RESUME, NULL, 1, portMAX_DELAY);
                    }
                }
            }

            if (bits & WIFI_EVENT_BIT_DO_CONNECT)
            {
                Log.infoln("[app_wifi] Event: start connecting...");

                app_wifi_connect_args_t local_connect_args{};

                if (xSemaphoreTake(g_wifi_args_mutex, 0) == pdTRUE)
                {
                    local_connect_args = g_wifi_connect_args;
                    xSemaphoreGive(g_wifi_args_mutex);
                }
                else
                {
                    Log.warningln("[app_wifi] Failed to acquire WiFi args mutex.");
                    return;
                }

                bool success = tryToConnect(local_connect_args.ssid, local_connect_args.password, true, false);

                wifi_connect_result_t result_data{};
                strlcpy(result_data.ssid, local_connect_args.ssid, sizeof(result_data.ssid));

                if (success)
                {
                    Log.infoln("[app_wifi] Connection successful, posting result to ATCMD.");
                    result_data.reason_code = 0;
                }
                else
                {
                    Log.errorln("[app_wifi] tryToConnect failed, posting result to ATCMD.");
                    result_data.reason_code = (lastDisconnectReason != 0) ? lastDisconnectReason : 201;
                }
                esp_event_post(CTRL_EVENT_BASE, APP_ATCMD_WIFI_CONNECT, &result_data, sizeof(result_data), portMAX_DELAY);
            }

            if (bits & WIFI_EVENT_BIT_NETWORK_CHECK)
            {
                if (WiFi.status() == WL_CONNECTED)
                {
                    Log.warningln("[app_wifi] Network Check: MQTT disconnected while WiFi is connected. Checking internet access...");
                    _is_stably_connected = false;
                    _network_check_active = true;
                    _network_check_ping_count = 0;
                    _next_ping_ms = now_ms;
                }
                else
                {
                    Log.infoln("[app_wifi] Network Check: Ignored, WiFi is not connected.");
                }
            }

            if (bits & WIFI_EVENT_BIT_SCAN_REQUEST)
            {
                if (!local_gallery || portal_running)
                {
                    start_scan(now_ms);
                }
                else
                {
                    Log.verboseln("[app_wifi] Gallery mode ignored background scan request.");
                }
            }
        }

        void handle_autoconnect(bool manual_request = false, bool show_image = true)
        {
            Log.infoln("[app_wifi] Event: Start Autoconnect%s", manual_request ? " (manual)" : "");
            if (g_provisioning_origin.load() == ProvisioningOrigin::Unknown)
            {
                g_provisioning_origin.store(ProvisioningOrigin::AutoConnect);
            }
            _is_stably_connected = false;
            _pending_autoconnect = false;
            _portal_connected = false;
            stopPortal();
            const AutoConnectResult result = attemptAutoConnect(manual_request);
            if (result == AutoConnectResult::Portal)
            {
                return;
            }

            set_provisioning_active(false);
            release_autoconnect_power();

            if (show_image &&
                (result == AutoConnectResult::Failed ||
                 (result == AutoConnectResult::NoCredentials && IsTimerWakeup())))
            {
                esp_event_post(VIEW_EVENT_BASE, VIEW_EVENT_SHOW_IMAGE, NULL, 1, portMAX_DELAY);
            }

            if (result == AutoConnectResult::Failed &&
                !IsLocalGallerySession() &&
                should_keep_online())
            {
                Log.warningln("[app_wifi] Auto-connect failed; retrying in %u ms.", WIFI_RETRY_BACKOFF_MS);
                _pending_autoconnect = true;
                _reconnect_due_ms = millis() + WIFI_RETRY_BACKOFF_MS;
            }
        }

        void handle_pending_reconnect(uint32_t now_ms)
        {
            if (!_pending_autoconnect)
            {
                return;
            }
            if (now_ms < _reconnect_due_ms)
            {
                return;
            }
            _pending_autoconnect = false;
            if (!should_keep_online())
            {
                Log.infoln("[app_wifi] Pending reconnect cancelled; device may enter deep sleep.");
                return;
            }
            handle_autoconnect(false, false);
        }

        void handle_pending_portal_done(uint32_t now_ms)
        {
            if (!_pending_portal_done)
            {
                return;
            }

            if (now_ms < _portal_done_due_ms)
            {
                return;
            }

            Log.infoln("[app_wifi] Portal complete");
            _pending_portal_done = false;
            stopPortal();
            set_provisioning_active(false);
            delay(1000);
            esp_event_post(CTRL_EVENT_BASE, SENSECRAFT_EVENT_START, NULL, 1, portMAX_DELAY);
        }

        void handle_network_check(uint32_t now_ms)
        {
            if (!_network_check_active)
            {
                return;
            }

            if (WiFi.status() != WL_CONNECTED)
            {
                Log.warningln("[app_wifi] Network Check: WiFi connection lost during ping check. Aborting.");
                _network_check_active = false;
                _network_check_ping_count = 0;
                return;
            }

            if (now_ms < _next_ping_ms)
            {
                return;
            }

            Log.infoln("[app_wifi] Network Check: Pinging 223.5.5.5...");
            if (Ping.ping("223.5.5.5"))
            {
                Log.infoln("[app_wifi] Network Check: Ping success! Internet is back.");
                esp_event_post(CTRL_EVENT_BASE, SENSECRAFT_EVENT_RESUME, NULL, 1, portMAX_DELAY);
                _is_stably_connected = true;
                _network_check_active = false;
                _network_check_ping_count = 0;
            }
            else
            {
                Log.errorln("[app_wifi] Network Check: Ping failed. Retrying in 5 seconds...");
                _next_ping_ms = now_ms + 5000;
                _network_check_ping_count++;
                if (_network_check_ping_count >= 10)
                {
                    Log.errorln("[app_wifi] Network check failed repeatedly.");
                    _network_check_active = false;
                    _network_check_ping_count = 0;

                    const bool keep_online = should_keep_online();
                    if (!IsLocalGallerySession() && keep_online)
                    {
                        Log.warningln("[app_wifi] Reconnecting WiFi in %u ms.", WIFI_RETRY_BACKOFF_MS);
                        WiFi.disconnect(true, false);
                        _pending_autoconnect = true;
                        _reconnect_due_ms = now_ms + WIFI_RETRY_BACKOFF_MS;
                    }
                    else if (!keep_online)
                    {
                        EnterDeepSleep();
                    }
                    else
                    {
                        Log.infoln("[app_wifi] Gallery mode: network reconnect skipped.");
                    }
                }
            }
        }

        void start_scan(uint32_t now_ms)
        {
            if (_scan_state != ScanState::Idle)
            {
                Log.warningln("[app_wifi] Scan request ignored, scan already in progress.");
                return;
            }

            Log.infoln("[app_wifi] Starting WiFi scan...");
            g_wifi_scan_cache.clear();

            WiFi.scanDelete();

            int result = WiFi.scanNetworks(true);
            if (result == WIFI_SCAN_RUNNING)
            {
                _scan_state = ScanState::Running;
                _scan_start_ms = now_ms;
            }
            else if (result >= 0)
            {
                finalize_scan(result);
            }
            else
            {
                Log.errorln("[app_wifi] Failed to start WiFi scan: %d", result);
                _scan_state = ScanState::Idle;
            }
        }

        void handle_scan_progress(uint32_t now_ms)
        {
            if (_scan_state != ScanState::Running)
            {
                return;
            }

            int status = WiFi.scanComplete();
            if (status == WIFI_SCAN_RUNNING)
            {
                if (now_ms - _scan_start_ms > 15000)
                {
                    Log.errorln("[app_wifi] WiFi scan timed out.");
                    WiFi.scanDelete();
                    _scan_state = ScanState::Idle;
                }
                return;
            }

            if (status == WIFI_SCAN_FAILED || status < 0)
            {
                Log.errorln("[app_wifi] WiFi scan failed: %d", status);
                WiFi.scanDelete();
                _scan_state = ScanState::Idle;
                return;
            }

            finalize_scan(status);
        }

        void finalize_scan(int count)
        {
            app_wifi_scan_done_args_t scan_results = {0};

            if (count > 0)
            {
                g_wifi_scan_cache.reserve(count);

                for (int i = 0; i < count; ++i)
                {
                    CachedAPInfo info;
                    strlcpy(info.ssid, WiFi.SSID(i).c_str(), sizeof(info.ssid));
                    memcpy(info.bssid, WiFi.BSSID(i), 6);
                    info.channel = WiFi.channel(i);
                    info.rssi = WiFi.RSSI(i);
                    info.authmode = WiFi.encryptionType(i);
                    g_wifi_scan_cache.push_back(info);
                }
            }
            else
            {
                Log.errorln("[app_wifi] No WiFi networks found during scan.");
            }

            WiFi.scanDelete();

            std::sort(g_wifi_scan_cache.begin(), g_wifi_scan_cache.end(), [](const CachedAPInfo &a, const CachedAPInfo &b)
                      { return a.rssi > b.rssi; });

            Log.infoln("[app_wifi] Scan complete, posting result event.");
            esp_event_post(WIFI_APP_EVENT_BASE, APP_WIFI_EVENT_SCAN_DONE, &scan_results, sizeof(scan_results), portMAX_DELAY);
            esp_event_post(CTRL_EVENT_BASE, APP_ATCMD_WIFI_SCAN, NULL, 1, portMAX_DELAY);

            _scan_state = ScanState::Idle;
        }

        void service_portal(uint32_t now_ms)
        {
            if (!portal_running)
            {
                return;
            }
            if (now_ms - _last_portal_service_ms < 10)
            {
                return;
            }
            _last_portal_service_ms = now_ms;
            dnsServer.processNextRequest();
            server.handleClient();
        }

        bool _started{false};
        bool _is_stably_connected{false};
        bool _is_first_connected{true};
        bool _pending_autoconnect{false};
        uint32_t _reconnect_due_ms{0};
        bool _pending_portal_done{false};
        uint32_t _portal_done_due_ms{0};
        bool _network_check_active{false};
        uint8_t _network_check_ping_count{0};
        uint32_t _next_ping_ms{0};
        ScanState _scan_state{ScanState::Idle};
        uint32_t _scan_start_ms{0};
        uint32_t _last_portal_service_ms{0};
    };

    WifiApp g_wifi_app;
    bool g_wifi_task_registered = false;

    void wifi_task()
    {
        g_wifi_app.run();
    }
}

void app_wifi_init()
{
    if (g_wifi_task_registered)
    {
        Log.warningln("[app_wifi] WiFi task already running.");
        return;
    }

    Log.infoln("[app_wifi] Initializing WiFi component...");

    WiFi.setAutoReconnect(false);
    wifi_event_group = xEventGroupCreate();
    g_wifi_args_mutex = xSemaphoreCreateMutex();

    esp_event_handler_register(WIFI_APP_EVENT_BASE, APP_WIFI_EVENT_START_AUTOCONNECT, &app_wifi_event_handler, NULL);
    esp_event_handler_register(WIFI_APP_EVENT_BASE, APP_WIFI_EVENT_START_PORTAL, &app_wifi_event_handler, NULL);
    esp_event_handler_register(WIFI_APP_EVENT_BASE, APP_WIFI_EVENT_PORTAL_DONE, &app_wifi_event_handler, NULL);
    esp_event_handler_register(WIFI_APP_EVENT_BASE, APP_WIFI_EVENT_CONNECT, &app_wifi_event_handler, NULL);
    esp_event_handler_register(WIFI_APP_EVENT_BASE, APP_WIFI_EVENT_SCAN, &app_wifi_event_handler, NULL);
    esp_event_handler_register(WIFI_APP_EVENT_BASE, APP_WIFI_EVENT_SCAN_DONE, &app_wifi_event_handler, NULL);
    esp_event_handler_register(WIFI_APP_EVENT_BASE, APP_MQTT_DISCONNECTED, &app_wifi_event_handler, NULL);
    esp_event_handler_register(APP_INPUT_EVENT_BASE, APP_INPUT_EVENT_ENTER_PROVISIONING, &app_wifi_input_event_handler, NULL);

    WiFi.onEvent(onWiFiEvent);

    esp_timer_create_args_t timer_args = {.callback = &_timer_cb_wifi};
    esp_err_t timer_err = esp_timer_create(&timer_args, &g_wifi_auto_timer);

    g_wifi_app.start(millis());

    if (!MiniRT::addTask(wifi_task, 10, MiniRT::Priority::Low))
    {
        Log.errorln("[app_wifi] Failed to register WiFi task with MiniRT");
        return;
    }

    g_wifi_task_registered = true;
    app_power_manager_acquire(AppPowerOwner::WifiConnect);
    g_autoconnect_power_held.store(true);

    if (timer_err == ESP_OK)
    {
        timer_err = esp_timer_start_once(g_wifi_auto_timer, 2000 * 1000);
    }

    if (timer_err != ESP_OK)
    {
        Log.warningln("[app_wifi] WiFi auto timer unavailable, starting autoconnect now: err=%d", timer_err);
        app_wifi_request_autoconnect(false);
    }
}
