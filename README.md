### ToolSense OpenWrt module test

#### I. Program description

The program can run in three work modes that are specified in the "device_status" structure:

* INIT SYS - initialization timer and network and communication protocols (MQTT, ModBus, OPC UA).

* PROCESS - one of main mode to communicate to peripheral instruments.

* SEND_DATA - Send data to MQTT server.

```
Configuration file location:
/etc/ts_module/ts_module.cfg
```

**INIT_SYS** 

* Init MQTT using setting in ts_module.cfg in “mqtt” section.

* Init Clients Manager and Queue 

* Init Timer using Linux interval timers

**PROCESS**

*soon*

**SEND_DATA**

After a set interval, the data's structure will be converted to "json" format and will send to MQTT server. The interval description in configuration file: "common" section, "send_interval_min" variable. MQTT Topic - “data”.

```
(WORK_DIR) - your work directory, for example /home/user1
```

### II a. Preparing, configuring and building the necessary tools using a precompiled SDK

Download a precompiled SDK. The precompiled SDK is found in the same download folder
```
https://downloads.openwrt.org/releases/
```
Download and decompressing the SDK archive. You can use command in *WORK_DIR*
```
curl -SL https://downloads.openwrt.org/releases/.../ openwrt-sdk-<Platform>_gcc-<version>_musl.Linux-x86_64.tar.xz | tar xJ
```
After decompressing the SDK archive, load package lists
```
./scripts/feeds update -a
```
Prepare the package and its dependencies
```
./scripts/feeds install libconfig
./scripts/feeds install libmbedtls
./scripts/feeds install libopenssl
./scripts/feeds install libbz2
./scripts/feeds install libmodbus
./scripts/feeds install libmosquitto
```
Set default configuration
```
make defconfig
```

#### II b. Preparing, configuring and building the necessary tools from sources

Cloned OpenWrt git repository:

```
cd (WORK_DIR)
git clone https://git.openwrt.org/openwrt/openwrt.git
```

Change into the source code directory
```
cd openwrt
```

Checkout a stable code revision and clean any possible build artifacts

```
git checkout v18.06.5 
make distclean
```

Update and install 'feeds' packages
```
./scripts/feeds update -a
./scripts/feeds install -a
```

Configure the cross-compilation toolchain by invoking the graphical configuration menu. From the menu, choose the suitable 'Target System', 'Subtarget' and 'Target Profile'. Choose 
```
make menuconfig
```

Build the target-independent tools and the cross-compilation toolchain
```
make toolchain/install
```
It is going to take a while

#### III. Cloning and building the owrt_module
Cloning source

```
cd (WORK_DIR)/openwrt/package
git clone https://github.com/ToolSense/owrt_module.git
```

Select package (If not  using a precompiled SDK)

```
cd (WORK_DIR)/openwrt
make menuconfig
```

*Select the “ToolSensePackages” sub-menu. Highlight the “ts_owrt_module” entry underneath this menu, and click on the 'Y' key to include this package into the firmware configuration.*

Building the owrt_module:

```
make package/owrt_module/compile
```

**If everything was done successfully, we will be presented with ts_owrt_module_<version>_<arch>.ipk in (WORK_DIR)/openwrt/bin/packages/<arch>/base   folder.**


#### IV. Deploying package

Installing FTP server on your router

```
opkg update
opkg install vsftpd
```

For installation purposes, you can save the package to the /tmp folder on your router (using  gftp or other ftp manager). Assuming you transferred the package to the /tmp folder, you can use the OPKG tool to install the package using the following command:

```
opkg install /tmp/ts_owrt_module_<version>_<arch>.ipk
```

#### V. Removing package

You can remove the installation of the package using the OPKG tool:
```
opkg remove ts_owrt_module
```  

#### VI. Program settings file

All program settings located in:
```
/etc/ts_module/ts_module.cfg
```
File automatically appear during program installation. It divided into sections: common, mqtt, opc_ua, clients.

**Section: common**  

Common settings for all program.

*Parameters:*

* send_interval_min - interval for data sending on mqtt server in minutes  
Example: send_interval_min = 60; - send data every hour

**Section: mqtt**  

Settings for MQTT server

*Parameters:*

* host - host name  
Example: host = "mqtt.toolsense.io";
	
* port - TCP port  
Example: port = 8883;
	
* login - mqtt login  
Example: login = "test";
	
* passwd - mqtt pass  
Example: passwd = "test";
	
* ssl_crt - path for ssl certificate  
Example: ssl_crt = "/etc/ts_module/mqtt.toolsense.io.cert";

**Section: opc_ua**  

Settings for OPC UA server  

*Parameters:*	

* server - server name  
Example: server = "opc.tcp://10.100.0.1:4840";
	
* secure - secure connection (Y - enable secure, N - no secure)  
Example: secure = "Y";
	
* security_mode - OPC UA secure mode (none, sign, sign_encrypt)  
Example: security_mode = "sign";
	
**Section: clients**  

Settings for each client  

*Parameters:*	

* name - client name, will be use to send data on MQTT server  
Example: "run"
	
* refresh_rate - interval between client polls (100ms, 1s, 1m)  
Example: refresh_rate = "1s";
	
* protocol - connection protocol (modbus, opc)  
Example: protocol = "opc"
	
* data_type - received data type (bool, int, dword, double, time, enum, char)  
Example: data_type = "int";
	
* unit - units of measure (h, kg, cm, m/s ...)  
Example: unit = "ms";
	
* counter - attached counter (difference, cumulated_not_100, cumulated)   
Modifies client data according to the selected algorithm
			
  + "difference" - Calculate the difference to the previous data, accumulate results. Erase when data sending.  
For example:  
counter = 0  
received client data = 10  
counter = 10  
received client data = 10  
counter = 10  
received client data = 30  
counter = 10'current val' + (30'client data' - 10'previous client data') = 30  

  + "cumulated_not_100" - Incremented by 1 each time client data != 100.  Erase when data sending.  
For example:  
counter = 0  
received client data = 101  
counter = 1  
received client data = 100  
counter = 1  

  + "cumulated" - Incremented by 1 when data changes. Erase when data sending.  
For example:  
counter = 0  
received client data = 100  
counter = 0 (because program just started)  
received client data = 200  
counter = 1  
received client data = 200  
counter = 1  

	
* computation - make computation with clients data or counter data if counter present (mul_60000000, mul_60, mul_1000)  
  + "mul_60"   -  multiplies value by 60  
  + "mul_1000" -  multiplies value by 1000  
  + "mul_60000000" -  multiplies value by 60000000  
Example: computation = "mul_60000000";
	
	
**Next parameters depend on protocol (opc or modbus)**
	
*For opc:*
	
* opc_ns - OPC node name space (1 ...)  
Example: opc_ns = 3;

* opc_node - OPC node name  
Example: opc_node = "\"D001_Kopplung_HMI\".\"Code_Bild_wechsel\"";
	
	
*For modbus:*
	
* mb_id - modbus id  
Example: mb_id = 1;
		
* mb_type - modbus connection type (TCP, RTU)  
Example: mb_type = "TCP";
		
**Next parameters depend on mb_type (TCP or RTU)**
		
*For TCP:*
		
* mb_ip - modbus IP adress  
Example: mb_ip = "192.168.1.226";
	
* mb_port - modbus TCP port  	
Example: mb_port = 502;
			
*For RTU:*
* mb_device - device name in linux /dev (/dev/ttyS0, /dev/ttyUSB0 ...)  
Example: mb_device = "/dev/ttyS0";
			
* mb_baud_rate - baud_rate (115200, 9600 ...)  
Example: mb_baud_rate = 115200;
			
* mb_parity - parity (N, E, O)  
Example: mb_parity = "N";
			
* mb_data_bit - data_bit (5 - 9)  
Example: mb_data_bit = 8;
			
* mb_stop_bit - stop_bit  
Example: mb_stop_bit = 1;
			
Example of ts_module.cfg, got 3 clients: 1 - opc, 2 - modbus TCP, 3 - modbus RTU  
```
common= ({  
	send_interval_min = 60;    #interval to send data  
});

mqtt = ({  
	host    = "mqtt.toolsense.io";  
	port    = 8883;  
	login   = "test";  
	passwd  = "test";  
	ssl_crt = "/etc/ts_module/mqtt.toolsense.io.cert";  
});

opc_ua = ({  
	server = "opc.tcp://10.100.0.1:4840";  
	secure = "Y";            # Y - enable secure, N - no secure  
	security_mode = "sign";  # none, sign, sign_encrypt  
});

clients =(  
{  
	name         = "client1";  
	refresh_rate = "1s";           # 100ms, 1s, 1m  
	protocol     = "opc"           # modbus, opc  
	data_type    = "int";          # bool, int, dword, double, time, enum, char  
	unit         = "ms";           # h, kg, cm, m/s ...  
	counter = "difference";        # none, difference, cumulated_not_100, cumulated  
	computation = "mul_60000000";  # none, mul_60, mul_1000, mul_60000000  
	
	opc_ns       = 3;              # node name space  
	opc_node     = "\"D001_Kopplung_HMI\".\"Betr_Min_Fahrt\"";       # node name  
},

{  
	name         = "client2";  
	refresh_rate = "1s";           # 100ms, 1s, 1m  
	protocol     = "modbus"        # modbus, opc  
	data_type    = "int";          # bool, int, dword, time, enum  
	unit         = "ms";           # h, kg, cm, m/s ...  
	counter      = "none";  

	mb_id        = 1;              # 1 ...  
	mb_type      = "TCP";          # TCP, RTU  
	
	#modbus TCP  
	mb_ip        = "192.168.1.226";  
	mb_port      = 502;  
},

{
	name         = "client_modbus";
	refresh_rate = "1s";             # 100ms, 1s, 1m
	protocol     = "modbus"          # modbus, opc
	data_type    = "int";            # bool, int, dword, time, enum 
	unit         = "ms";             # h, kg, cm, m/s ...
	counter      = "none";
 
 	mb_id        = 2;                # 1 ...
	mb_type      = "RTU";            # TCP, RTU
	
	#modbus RTU
	mb_device    = "/dev/ttyS0";    # /dev/ttyS0 ...
	mb_baud_rate = 115200;          # 115200, 9600 ...
	mb_parity    = "N";             # N, Y
	mb_data_bit  = 8;
	mb_stop_bit  = 1;
}
);
```
