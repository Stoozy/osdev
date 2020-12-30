#include <string.h>
#include <stdio.h>

#include <kernel/io.h>
#include <kernel/rtc.h>
#include <kernel/util.h>

datetime_t *current_time;

char ret[50];
bool updating(){
    outb(CMOS_ADDR, 0x0A);
    uint32_t status = inb(CMOS_DATA);
    return (status & 0x80);
}

uint8_t _read_rtc_reg(uint8_t reg){
	outb(CMOS_ADDR, reg);
	uint8_t in  = inb(CMOS_DATA);
	return in;
}

void _set_rtc_register(uint16_t reg_num, uint8_t val) {
    outb(CMOS_ADDR, reg_num);
    outb(CMOS_DATA, val);
}

char * datetime_to_str(datetime_t * dt){	
	ret[49] = '\0';
	char day[4];
    char hour[3];
    char min[3];
    char sec[3];

	hour[0] = '0';
	min[0] = '0';
	sec[0] = '0';
	char * ret_ptr = &ret[0];

	char * ptr = &hour[0];

	itoa(current_time->hour, hour, 10);
	itoa(current_time->min, min, 10);
	itoa(current_time->sec, sec, 10);

	strcat(ret_ptr, ptr);
	strcat(ret_ptr, ":");

	ptr = &min[0];
	strcat(ret_ptr, ptr);
	strcat(ret_ptr, ":");

	ptr = &sec[0];
	strcat(ret_ptr, ptr);


	printf("%s UTC \n", ret);
	for(int i=0; i<49; i++){
		ret[i] = 0;
	}
	return &ret[0];
}

void read_rtc(){

	current_time->sec = _read_rtc_reg(0x00);
	current_time->min = _read_rtc_reg(0x02);
	current_time->hour = _read_rtc_reg(0x04);
	current_time->day = _read_rtc_reg(0x06);	// weekday
	current_time->date = _read_rtc_reg(0x07);
	current_time->year = _read_rtc_reg(0x09);

	uint8_t registerB = _read_rtc_reg(0x0B);
    if (!(registerB & 0x04)) {
        current_time->sec = (current_time->sec & 0x0F) + ((current_time->sec / 16) * 10);

        current_time->min = (current_time->min & 0x0F) + ((current_time->min / 16) * 10);

        current_time->hour = ( (current_time->hour & 0x0F) + (((current_time->hour & 0x70) / 16) * 10) ) | (current_time->hour & 0x80);

        current_time->day = (current_time->day & 0x0F) + ((current_time->day / 16) * 10);
        current_time->month = (current_time->month & 0x0F) + ((current_time->month / 16) * 10);
        current_time->year = (current_time->year & 0x0F) + ((current_time->year / 16) * 10);
    }

	datetime_to_str(current_time);
}

datetime_t get_time(){
	
	read_rtc();
	return *current_time;
}

