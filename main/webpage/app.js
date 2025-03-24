/**
 * Add gobals here
 */
var seconds 	= null;
var otaTimerVar =  null;
var wifiConnectInterval = null;

// Ethernet connection status timer
var ethernetStatusTimerID;
// Timer interval for connection status
var ethernetStatusInterval = 2000;

/**
 * Initialize functions here.
 */
$(document).ready(function(){
	getSSID();
	getUpdateStatus();
	//startDHTSensorInterval();
	//startMd02SensorInterval();
	startLocalTimeInterval();
	// Mengecek status koneksi awal
    getEthernetConfigInfo();
    getEthernetConnectionStatus();
	getConnectInfo();
	$("#connect_wifi").on("click", function(){
		checkCredentials();
	}); 
	$("#disconnect_wifi").on("click", function(){
		disconnectWifi();
	}); 
	// Menambahkan event listener untuk tombol
    $('#ethConnectButton').click(connectEthernet);
    $('#ethDisconnectButton').click(disconnectEthernet);
});   

/**
 * Gets file name and size for display on the web page.
 */        
function getFileInfo() 
{
    var x = document.getElementById("selected_file");
    var file = x.files[0];

    document.getElementById("file_info").innerHTML = "<h4>File: " + file.name + "<br>" + "Size: " + file.size + " bytes</h4>";
}

/**
 * Handles the firmware update.
 */
function updateFirmware() 
{
    // Form Data
    var formData = new FormData();
    var fileSelect = document.getElementById("selected_file");
    
    if (fileSelect.files && fileSelect.files.length == 1) 
	{
        var file = fileSelect.files[0];
        formData.set("file", file, file.name);
        document.getElementById("ota_update_status").innerHTML = "Uploading " + file.name + ", Firmware Update in Progress...";

        // Http Request
        var request = new XMLHttpRequest();

        request.upload.addEventListener("progress", updateProgress);
        request.open('POST', "/OTAupdate");
        request.responseType = "blob";
        request.send(formData);
    } 
	else 
	{
        window.alert('Select A File First')
    }
}

/**
 * Progress on transfers from the server to the client (downloads).
 */
function updateProgress(oEvent) 
{
    if (oEvent.lengthComputable) 
	{
        getUpdateStatus();
    } 
	else 
	{
        window.alert('total size is unknown')
    }
}

/**
 * Posts the firmware udpate status.
 */
function getUpdateStatus() 
{
    var xhr = new XMLHttpRequest();
    var requestURL = "/OTAstatus";
    xhr.open('POST', requestURL, false);
    xhr.send('ota_update_status');

    if (xhr.readyState == 4 && xhr.status == 200) 
	{		
        var response = JSON.parse(xhr.responseText);
						
	 	document.getElementById("latest_firmware").innerHTML = response.compile_date + " - " + response.compile_time

		// If flashing was complete it will return a 1, else -1
		// A return of 0 is just for information on the Latest Firmware request
        if (response.ota_update_status == 1) 
		{
    		// Set the countdown timer time
            seconds = 10;
            // Start the countdown timer
            otaRebootTimer();
        } 
        else if (response.ota_update_status == -1)
		{
            document.getElementById("ota_update_status").innerHTML = "!!! Upload Error !!!";
        }
    }
}

/**
 * Displays the reboot countdown.
 */
function otaRebootTimer() 
{	
    document.getElementById("ota_update_status").innerHTML = "OTA Firmware Update Complete. This page will close shortly, Rebooting in: " + seconds;

    if (--seconds == 0) 
	{
        clearTimeout(otaTimerVar);
        window.location.reload();
    } 
	else 
	{
        otaTimerVar = setTimeout(otaRebootTimer, 1000);
    }
}

/**
 * Gets DHT22 sensor temperature and humidity values for display on the web page.
 */
function getDHTSensorValues()
{
	$.getJSON('/dhtSensor.json', function(data) {
		$("#temperature_reading").text(data["temp"]);
		$("#humidity_reading").text(data["humidity"]);
	});
}

/**
 * Sets the interval for getting the updated DHT22 sensor values.
 */
function startDHTSensorInterval()
{
	setInterval(getDHTSensorValues, 5000);    
}

/**
 * Gets DHT22 sensor temperature and humidity values for display on the web page.
 */
function getMd02SensorValues()
{
	$.getJSON('/md02Sensor.json', function(data) {
		$("#temperature_reading").text(data["temp"]);
		$("#humidity_reading").text(data["humidity"]);
	});
}

/**
 * Sets the interval for getting the updated DHT22 sensor values.
 */
function startMd02SensorInterval()
{
	setInterval(getMd02SensorValues, 5000);    
}

/**
 * Clears the connection status interval.
 */
function stopWifiConnectStatusInterval()
{
	if (wifiConnectInterval != null)
	{
		clearInterval(wifiConnectInterval);
		wifiConnectInterval = null;
	}
}

/**
 * Gets the WiFi connection status.
 */
function getWifiConnectStatus()
{
	var xhr = new XMLHttpRequest();
	var requestURL = "/wifiConnectStatus";
	xhr.open('POST', requestURL, false);
	xhr.send('wifi_connect_status');
	
	if (xhr.readyState == 4 && xhr.status == 200)
	{
		var response = JSON.parse(xhr.responseText);
		
		document.getElementById("wifi_connect_status").innerHTML = "Connecting...";
		
		if (response.wifi_connect_status == 2)
		{
			document.getElementById("wifi_connect_status").innerHTML = "<h4 class='rd'>Failed to Connect. Please check your AP credentials and compatibility</h4>";
			stopWifiConnectStatusInterval();
		}
		else if (response.wifi_connect_status == 3)
		{
			document.getElementById("wifi_connect_status").innerHTML = "<h4 class='gr'>Connection Success!</h4>";
			stopWifiConnectStatusInterval();
			getConnectInfo();
		}
	}
}

/**
 * Starts the interval for checking the connection status.
 */
function startWifiConnectStatusInterval()
{
	wifiConnectInterval = setInterval(getWifiConnectStatus, 2800);
}

/**
 * Connect WiFi function called using the SSID and password entered into the text fields.
 */
function connectWifi() {
    // Get the SSID and password
    selectedSSID = $("#connect_ssid").val();
    pwd = $("#connect_pass").val();
    
    // Batasi panjang SSID dan password untuk menghindari header terlalu besar
    if (selectedSSID.length > 32) { // 32 adalah batas maksimum SSID WiFi
        selectedSSID = selectedSSID.substring(0, 32);
    }
    if (pwd.length > 64) { // 64 adalah batas maksimum password WiFi
        pwd = pwd.substring(0, 64);
    }
    
    $.ajax({
        url: '/wifiConnect.json',
        dataType: 'json',
        method: 'POST',
        cache: false,
        headers: {
            'my-connect-ssid': selectedSSID,
            'my-connect-pwd': pwd
        },
        error: function(xhr, status, error) {
            // Handle error
            stopWifiConnectStatusInterval();
        }
    });
    
    startWifiConnectStatusInterval();
}

/**
 * Checks credentials on connect_wifi button click.
 */
function checkCredentials() {
    errorList = "";
    credsOk = true;
    
    selectedSSID = $("#connect_ssid").val();
    pwd = $("#connect_pass").val();
    
    // Validasi panjang SSID dan password
    if (selectedSSID == "") {
        errorList += "<h4 class='rd'>SSID cannot be empty!</h4>";
        credsOk = false;
    } else if (selectedSSID.length > 32) {
        errorList += "<h4 class='rd'>SSID too long (max 32 characters)!</h4>";
        credsOk = false;
    }
    
    if (pwd == "") {
        errorList += "<h4 class='rd'>Password cannot be empty!</h4>";
        credsOk = false;
    } else if (pwd.length > 64) {
        errorList += "<h4 class='rd'>Password too long (max 64 characters)!</h4>";
        credsOk = false;
    }
    
    if (credsOk == false) {
        $("#wifi_connect_credentials_errors").html(errorList);
    } else {
        $("#wifi_connect_credentials_errors").html("");
        connectWifi();    
    }
}

/**
 * Shows the WiFi password if the box is checked.
 */
function showPassword()
{
	var x = document.getElementById("connect_pass");
	if (x.type === "password")
	{
		x.type = "text";
	}
	else
	{
		x.type = "password";
	}
}

/**
 * Gets the connection information for displaying on the web page.
 */
function getConnectInfo()
{
	$.getJSON('/wifiConnectInfo.json', function(data)
	{
		$("#connected_ap_label").html("Connected to: ");
		$("#connected_ap").text(data["ap"]);
		
		$("#ip_address_label").html("IP Address: ");
		$("#wifi_connect_ip").text(data["ip"]);
		
		$("#netmask_label").html("Netmask: ");
		$("#wifi_connect_netmask").text(data["netmask"]);
		
		$("#gateway_label").html("Gateway: ");
		$("#wifi_connect_gw").text(data["gw"]);
		
		document.getElementById('disconnect_wifi').style.display = 'block';
	});
}

/**
 * Disconnects Wifi once the disconnect button is pressed and reloads the web page.
 */
function disconnectWifi()
{
	$.ajax({
		url: '/wifiDisconnect.json',
		dataType: 'json',
		method: 'DELETE',
		cache: false,
		data: { 'timestamp': Date.now() }
	});
	// Update the web page
	setTimeout("location.reload(true);", 2000);
}

/**
 * Sets the interval for displaying local time.
 */
function startLocalTimeInterval()
{
	setInterval(getLocalTime, 10000);
}

/**
 * Gets the local time.
 * @note connect the ESP32 to the internet and the time will be updated.
 */
function getLocalTime()
{
	$.getJSON('/localTime.json', function(data) {
		$("#local_time").text(data["time"]);
	});
}

/**
 * Gets the ESP32's access point SSID for displaying on the web page.
 */
function getSSID()
{
	$.getJSON('/apSSID.json', function(data) {
		$("#ap_ssid").text(data["ssid"]);
	});
}


/**
 * Inisialisasi koneksi Ethernet
 */
function getEthernetConfigInfo() {
    $.getJSON('/ethConfig.json', function(response) {
        // Mengatur radio button sesuai mode
        if (response.mode === 0) { // Static IP
            $('#staticIPRadio').prop('checked', true);
            $('#dhcpRadio').prop('checked', false);
            enableStaticIPFields(true);
        } else { // DHCP
            $('#dhcpRadio').prop('checked', true);
            $('#staticIPRadio').prop('checked', false);
            enableStaticIPFields(false);
        }

        // Mengisi input field untuk IP statis
        $('#staticIP').val(response.ip);
        $('#staticSubnet').val(response.subnet);
        $('#staticGateway').val(response.gateway);
        
        // Tampilkan MAC Address
        $('#macAddress').text(response.mac);
    });
}

/**
 * Menghubungkan ke Ethernet
 */
function connectEthernet() {
    // Dapatkan mode IP (DHCP atau Static)
    var ipMode = $('input[name="ipMode"]:checked').val();
    
    // Buat header HTTP request
    var headers = {
        'ip-mode': ipMode
    };
    
    // Jika mode statis, tambahkan header untuk konfigurasi IP
    if (ipMode === 'static') {
        headers['static-ip'] = $('#staticIP').val();
        headers['static-subnet'] = $('#staticSubnet').val();
        headers['static-gateway'] = $('#staticGateway').val();
        
        // Validasi IP address
        if (!validateIPAddress(headers['static-ip']) || 
            !validateIPAddress(headers['static-subnet']) || 
            !validateIPAddress(headers['static-gateway'])) {
            alert('Please enter valid IP addresses');
            return;
        }
    }
    
    // Kirim request untuk menghubungkan Ethernet
    $.ajax({
        url: '/ethConnect.json',
        method: 'POST',
        headers: headers,
        dataType: 'json',
        success: function() {
            // Mulai polling status koneksi
            startEthernetStatusTimer();
            $('#ethConnectButton').prop('disabled', true);
            $('#ethDisconnectButton').prop('disabled', false);
            $('#statusMessage').text('Connecting to Ethernet...');
        },
        error: function(xhr) {
            alert('Failed to connect: ' + xhr.responseText);
        }
    });
}

/**
 * Memutuskan koneksi Ethernet
 */
function disconnectEthernet() {
    $.ajax({
        url: '/ethDisconnect.json',
        method: 'DELETE',
        dataType: 'json',
        success: function() {
            clearTimeout(ethernetStatusTimerID);
            $('#ethConnectButton').prop('disabled', false);
            $('#ethDisconnectButton').prop('disabled', true);
            $('#statusMessage').text('Disconnected from Ethernet');
            $('#ipAddress').text('');
            $('#subnetMask').text('');
            $('#gateway').text('');
        }
    });
}

/**
 * Memulai timer untuk memeriksa status koneksi
 */
function startEthernetStatusTimer() {
    ethernetStatusTimerID = setTimeout(function() {
        getEthernetConnectionStatus();
    }, ethernetStatusInterval);
}

/**
 * Mendapatkan status koneksi Ethernet
 */
function getEthernetConnectionStatus() {
    $.ajax({
        url: '/ethConnectStatus',
        method: 'POST',
        dataType: 'json',
        success: function(response) {
            var EthStatus = {
                0: 'Idle',
                1: 'Connecting...',
                2: 'Failed',
                3: 'Connected',
                4: 'Disconnected'
            };
            
            // Update status text
            $('#statusMessage').text('Status: ' + EthStatus[response.eth_connect_status]);
            
            // Enable/disable buttons based on status
            if (response.eth_connect_status === 3) { // Connected
                $('#ethConnectButton').prop('disabled', true);
                $('#ethDisconnectButton').prop('disabled', false);
                // Get IP information
                getEthernetConnectionInfo();
                // Stop polling
                clearTimeout(ethernetStatusTimerID);
            } else if (response.eth_connect_status === 2) { // Failed
                $('#ethConnectButton').prop('disabled', false);
                $('#ethDisconnectButton').prop('disabled', true);
                // Stop polling
                clearTimeout(ethernetStatusTimerID);
            } else if (response.eth_connect_status === 1) { // Connecting
                // Continue polling
                ethernetStatusTimerID = setTimeout(function() {
                    getEthernetConnectionStatus();
                }, ethernetStatusInterval);
            }
        }
    });
}

/**
 * Mendapatkan informasi koneksi Ethernet (IP, subnet, gateway)
 */
function getEthernetConnectionInfo() {
    $.getJSON('/ethConnectInfo.json', function(response) {
        if (response.ip) {
            $('#ipAddress').text(response.ip);
            $('#subnetMask').text(response.netmask);
            $('#gateway').text(response.gw);
            $('#connectionType').text(response.mode);
        }
    });
}

/**
 * Memvalidasi format alamat IP
 */
function validateIPAddress(ip) {
    var ipPattern = /^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$/;
    var ipMatch = ip.match(ipPattern);
    
    if (ipMatch) {
        for (var i = 1; i <= 4; i++) {
            if (parseInt(ipMatch[i]) > 255) {
                return false;
            }
        }
        return true;
    }
    
    return false;
}

/**
 * Enable/disable field input IP statis
 */
function enableStaticIPFields(enable) {
    $('#staticIP').prop('disabled', !enable);
    $('#staticSubnet').prop('disabled', !enable);
    $('#staticGateway').prop('disabled', !enable);
}

// Event handler untuk perubahan radio button
$('input[name="ipMode"]').change(function() {
    var mode = $(this).val();
    enableStaticIPFields(mode === 'static');
});






    










    

