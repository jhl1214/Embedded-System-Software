#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include "fpga_dot_font.h"

void Java_com_example_androidex_TextActivity_TextEditor (JNIEnv *env, jobject thiz, jstring string){
	int text_dev, fpga_fnd, fpga_dot, fpga_led;
	int i, length, str_size, text_size;
	unsigned char led;
	unsigned char data[4];
	unsigned char text[32];

	// Open fpga text lcd driver
	if((text_dev = open("/dev/fpga_text_lcd", O_WRONLY)) < 0)
		perror("/dev/fpga_text_lcd open error");
	memset(text, 0, sizeof(text));

	// Open fpga fnd driver
	if((fpga_fnd = open("/dev/fpga_fnd", O_WRONLY)) < 0)
		perror("/dev/fpga_fnd open error");

	// Open fpga dot driver
	if((fpga_dot = open("/dev/fpga_dot", O_WRONLY)) < 0)
		perror("/dev/fpga_dot open error");
	str_size = sizeof(fpga_number[18]);

	// Open fpga led driver
	if((fpga_led = open("/dev/fpga_led", O_RDWR)) < 0)
		perror("/dev/fpga_led open error");

	// Conver jstring to c string
	const char *str = (*env)->GetStringUTFChars(env, string, 0);
	length = (*env)->GetStringLength(env, string);

	// Clear unnecessary chars
	text_size = strlen(str);
	if(text_size > 0){
		strncat(text, str, text_size);
		memset(text+text_size, ' ', 32-text_size);
	}

	// Convert integer to string format
	sprintf(data, "%04d", length);
	led = atoi(data);

	// Get last number of length
	length = length % 10;

	// Print on devices
	write(text_dev, text, 32);	// fpga text lcd
	write(fpga_fnd, &data, 4);	// fpga fnd
	if(str[0] == '\0')
		write(fpga_dot, fpga_set_blank, str_size);
	else
		write(fpga_dot, fpga_number[length], str_size);
	write(fpga_led, &led, 1);

	// Free memory allocated for the string
	(*env)->ReleaseStringUTFChars(env, string, str);

	// Close device driver
	close(text_dev);
	close(fpga_fnd);
	close(fpga_dot);
	close(fpga_led);
}       