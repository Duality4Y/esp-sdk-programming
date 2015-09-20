#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
#include "user_config.h"
#include "user_interface.h"
#include "driver/uart.h"

#define user_procTaskPrio        0
#define user_procTaskQueueLen    1
os_event_t    user_procTaskQueue[user_procTaskQueueLen];
static void loop(os_event_t *events);

extern UartDevice UartDev;
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

//Main code function
static void ICACHE_FLASH_ATTR
loop(os_event_t *events)
{
    // struct ip_info *info;
    // if(wifi_get_ip_info(STATION_IF, info))
    // {
    //     os_printf("ip: "IPSTR"\n");
    //     // os_printf("netmask: "IPSTR"\n", IP2STR(info.ip));
    //     // os_printf("gw: "IPSTR"\n", IP2STR(info.ip));
    // }
    os_printf("Hello\n\r");
    // uart0_sendStr("Hello\n\r");
    static int i;
    for(i = 0; i < 1000; i++)
    {
        os_delay_us(1000);
    }
    system_os_post(user_procTaskPrio, 0, 0 );
}

//Init function 
void ICACHE_FLASH_ATTR
user_init()
{
    char ssid[32] = SSID;
    char password[64] = SSID_PASSWORD;
    struct station_config stationConf;

    //Set station mode
    wifi_set_opmode( 0x1 );

    // setup uart com.
    UartDev.data_bits = EIGHT_BITS;
    UartDev.parity = NONE_BITS;
    UartDev.stop_bits = ONE_STOP_BIT;
    uart_init(BIT_RATE_115200, BIT_RATE_115200);
    os_install_putc1(writebyte);

    //Set ap settings
    os_memcpy(&stationConf.ssid, ssid, 32);
    os_memcpy(&stationConf.password, password, 64);
    wifi_station_set_config(&stationConf);

    //Start os task
    system_os_task(loop, user_procTaskPrio,user_procTaskQueue, user_procTaskQueueLen);

    system_os_post(user_procTaskPrio, 0, 0 );
}
