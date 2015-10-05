#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
#include "user_config.h"
#include "user_interface.h"
#include "driver/uart.h"

#define BAUD (250000)

#define STARTOFDATA (0x02)
#define ENDOFDATA (0x03)

#define user_procTaskPrio        0
#define user_procTaskQueueLen    1
os_event_t user_procTaskQueue[user_procTaskQueueLen];
static void loop(os_event_t *events);

extern UartDevice UartDev;

static volatile os_timer_t uart_send_timer;

/*
 function that together with 
 os_install_getc1
 lets me print with os_printf
 source:
 http://bbs.espressif.com/viewtopic.php?t=218
*/
void writebyte(uint8_t b)
{
  while (true)
  {
     uint32_t fifo_cnt = READ_PERI_REG(UART_STATUS(0)) & (UART_TXFIFO_CNT << UART_TXFIFO_CNT_S);
     if ((fifo_cnt >> UART_TXFIFO_CNT_S & UART_TXFIFO_CNT) < 126)
        break;
  }
  WRITE_PERI_REG(UART_FIFO(0) , b);
}

void print_ip(char *desc, uint32 addr)
{
    static char str[100];
    os_sprintf(str, "%s", desc);
    uart0_sendStr(str);
    if(addr>0)
    {
        os_sprintf(str, "%d.%d.%d.%d", IP2STR(&addr));
        uart0_sendStr(str);
    }
    else
    {
        os_sprintf(str, "%s", "Not set");
        uart0_sendStr(str);
    }
    os_sprintf(str, "%s", "\n\r");
    uart0_sendStr(str);
}

void show_connection()
{
    static char str[100];
    os_sprintf(str, "%s", "\n\rCurrent Details\n\r");
    uart0_sendStr(str);

    // Show MAC
    char mac_address[32];
    wifi_get_macaddr(0, &mac_address[0]);
    os_sprintf(str, "MAC Address %02x-%02x-%02x-%02x-%02x-%02x\n\r", 
        (unsigned)mac_address[0], (unsigned)mac_address[1],
        (unsigned)mac_address[2], (unsigned)mac_address[3],    
        (unsigned)mac_address[4], (unsigned)mac_address[5]);
    uart0_sendStr(str);

    // Get Status
    char stat[100];
    uint8_t status = wifi_station_get_connect_status();
    switch(status)
    {
        case STATION_IDLE:
            os_sprintf(stat, "%s", "STATION_IDLE");
            break;
        case STATION_CONNECTING:
            os_sprintf(stat, "%s", "STATION_CONNECTING");
            break;
        case STATION_WRONG_PASSWORD:
            os_sprintf(stat, "%s", "STATION_WRONG_PASSWORD");
            break;
        case STATION_NO_AP_FOUND:
            wifi_station_connect();
            os_sprintf(stat, "%s", "STATION_NO_AP_FOUND");
            break;
        case STATION_CONNECT_FAIL:
            wifi_station_connect();
            os_sprintf(stat, "%s", "STATION_CONNECT_FAIL");
            break;
        case STATION_GOT_IP:
            os_sprintf(stat, "%s", "STATION_GOT_IP");
            break;
        default:
            os_sprintf(stat, "%s", "Unknown");
            break;
    }
    os_sprintf(str, "Connect status - %d: %s\n\r", status, stat);
    uart0_sendStr(str);

    // Get Config
    struct station_config current_config;
    wifi_station_get_config(&current_config);
    os_sprintf(str, "Curent Config (Station - %s, Paswword - %s)\n\r", 
        current_config.ssid, current_config.password);
    uart0_sendStr(str);

    // Get IP Info
    struct ip_info ipinfo;
    ipinfo.ip.addr=0;
    ipinfo.netmask.addr=255;
    ipinfo.gw.addr=0;
    wifi_get_ip_info(0, &ipinfo);

    print_ip("IP Address: ", ipinfo.ip.addr);
    print_ip("Netmask   : ", ipinfo.netmask.addr);
    print_ip("Gateway   : ", ipinfo.gw.addr);

    if(wifi_station_set_hostname("elite"))
    {
        os_sprintf(str, "received hostname: %s", wifi_station_get_hostname());
    }
}

void uart_send(void *arg)
{
    writebyte(0x82);
    int i;
    for(i = 0; i < 80; i++)
    {
        writebyte(i);
    }
    writebyte(0x83);
}

//Main code function
inline static void
loop(os_event_t *e)
{
    // uart_send(NULL);
    system_os_post(user_procTaskPrio, 0, 0 );
}

//Init function 
void ICACHE_FLASH_ATTR
user_init()
{
    system_set_os_print(false);
    os_install_putc1(writebyte);

    //Set ap settings
    char ssid[32] = SSID;
    char password[64] = SSID_PASSWORD;
    struct station_config stationConf;

    //Set station mode
    wifi_set_opmode(0x1);
    wifi_set_opmode_current(0x1);
    // need not check mac address of AP
    stationConf.bssid_set = 0;
    // copy ssid and password and set the config
    os_memcpy(&stationConf.ssid, ssid, 32);
    os_memcpy(&stationConf.password, password, 64);
    wifi_station_set_config(&stationConf);
    wifi_station_connect();
    wifi_station_dhcpc_start();
    wifi_station_set_hostname("elite");

    // setup uart com.
    UartDev.data_bits = EIGHT_BITS;
    UartDev.parity = NONE_BITS;
    UartDev.stop_bits = ONE_STOP_BIT;
    //uart_init(250000, 250000);
    uart_init(BAUD, BAUD);

    // setup a timer to send things on uart periodicly (update matrix)
    os_timer_disarm(&uart_send_timer);
    // setup timer
    os_timer_setfn(&uart_send_timer, (os_timer_func_t *)uart_send, NULL);
    // arm timer
    os_timer_arm(&uart_send_timer, 200, 1);

    //Start os task
    system_os_task(loop, user_procTaskPrio,user_procTaskQueue, user_procTaskQueueLen);
    system_os_post(user_procTaskPrio, 0, 0 );
}
