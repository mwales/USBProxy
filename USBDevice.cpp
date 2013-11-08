/*
 * Copyright 2013 Dominic Spill
 * Copyright 2013 Adam Stasiak
 *
 * This file is part of USB-MitM.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#include <linux/types.h>
#include "USBDevice.h"
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>

//TODO: update active endpoints upon set configuration request
//TODO: pull current config from proxy

USBDevice::USBDevice(USBDeviceProxy* _proxy) {
	proxy=_proxy;
	__u8 buf[18];
	usb_ctrlrequest setup_packet;
	setup_packet.bRequestType=USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE;
	setup_packet.bRequest=USB_REQ_GET_DESCRIPTOR;
	setup_packet.wValue=USB_DT_DEVICE<<8;
	setup_packet.wIndex=0;
	setup_packet.wLength=18;
	int len=0;
	proxy->control_request(&setup_packet,&len,buf);
	memcpy(&descriptor,buf,len);
	int i;
	configurations=(USBConfiguration **)calloc(descriptor.bNumConfigurations,sizeof(*configurations));

	maxStringIdx=(descriptor.iManufacturer>maxStringIdx)?descriptor.iManufacturer:maxStringIdx;
	maxStringIdx=(descriptor.iProduct>maxStringIdx)?descriptor.iProduct:maxStringIdx;
	maxStringIdx=(descriptor.iSerialNumber>maxStringIdx)?descriptor.iSerialNumber:maxStringIdx;
	strings=(USBString ***)calloc(maxStringIdx+1,sizeof(*strings));

	add_string(0,0);

	if (descriptor.iManufacturer) {add_string(descriptor.iManufacturer);}
	if (descriptor.iProduct) {add_string(descriptor.iProduct);}
	if (descriptor.iSerialNumber) {add_string(descriptor.iSerialNumber);}

	for(i=0;i<descriptor.bNumConfigurations;i++) {
		configurations[i]=new USBConfiguration(proxy,i);
		__u8 iConfiguration=configurations[i]->get_descriptor()->iConfiguration;
		if (iConfiguration) {add_string(iConfiguration);}
		configurations[i]->set_usb_device(this);
		int j;
		for (j=0;j<configurations[i]->get_descriptor()->bNumInterfaces;j++) {
			int k;
			for (k=0;k<configurations[i]->get_interface_alernate_count(j);k++) {
				__u8 iInterface=configurations[i]->get_interface(j,k)->get_descriptor()->iInterface;
				if (iInterface) {add_string(iInterface);}
			}
		}
	}
	//TODO: read child string descriptors

	//TODO: read high speed configs
}

USBDevice::USBDevice(usb_device_descriptor* _descriptor) {
	proxy=NULL;
	descriptor=*_descriptor;
	configurations=(USBConfiguration **)calloc(descriptor.bNumConfigurations,sizeof(*configurations));
	strings=(USBString ***)calloc(1,sizeof(*strings));
	strings[0]=(USBString **)malloc(sizeof(**strings)*2);
	char16_t zero[]={0x0409, 0};
	strings[0][0]=new USBString(zero,0,0);
	strings[0][1]=NULL;
}

USBDevice::USBDevice(__le16 bcdUSB,	__u8  bDeviceClass,	__u8  bDeviceSubClass,	__u8  bDeviceProtocol,	__u8  bMaxPacketSize0,	__le16 idVendor,	__le16 idProduct,	__le16 bcdDevice,	__u8  iManufacturer,	__u8  iProduct,	__u8  iSerialNumber,	__u8  bNumConfigurations) {
	proxy=NULL;
	descriptor.bcdUSB=bcdUSB;
	descriptor.bDeviceClass=bDeviceClass;
	descriptor.bDeviceSubClass=bDeviceSubClass;
	descriptor.bDeviceProtocol=bDeviceProtocol;
	descriptor.bMaxPacketSize0=bMaxPacketSize0;
	descriptor.idVendor=idVendor;
	descriptor.idProduct=idProduct;
	descriptor.bcdDevice=bcdDevice;
	descriptor.iManufacturer=iManufacturer;
	descriptor.iProduct=iProduct;
	descriptor.iSerialNumber=iSerialNumber;
	descriptor.bNumConfigurations=bNumConfigurations;
	configurations=(USBConfiguration **)calloc(descriptor.bNumConfigurations,sizeof(*configurations));
	strings=(USBString ***)calloc(1,sizeof(*strings));
	strings[0]=(USBString **)malloc(sizeof(**strings)*2);
	char16_t zero[]={0x0409, 0};
	strings[0][0]=new USBString(zero,0,0);
	strings[0][1]=NULL;
}

USBDevice::~USBDevice() {
	int i;
	for(i=0;i<descriptor.bNumConfigurations;i++) {delete(configurations[i]);}
	free(configurations);
	//todo: free strings
}

const usb_device_descriptor* USBDevice::get_descriptor() {
	return &descriptor;
};

void USBDevice::add_configuration(USBConfiguration* config) {
	int value=config->get_descriptor()->bConfigurationValue-1;
	if (configurations[value]) {delete(configurations[value]);}
	configurations[value]=config;
}

USBConfiguration* USBDevice::get_configuration(__u8 index) {
	return configurations[index-1];
}

void USBDevice::print(__u8 tabs) {
	int i;
	for(i=0;i<tabs;i++) {putchar('\t');}
	printf("Device:");
	for(i=0;i<sizeof(descriptor);i++) {printf(" %02x",((__u8 *)&descriptor)[i]);}
	putchar('\n');
	USBString* s;
	if (descriptor.iManufacturer) {
		s=get_manufacturer_string();
		if (s) {
			for(i=0;i<tabs;i++) {putchar('\t');}
			printf("  Manufacturer: ");
			s->print_ascii(stdout);
			putchar('\n');
		}
	}
	if (descriptor.iProduct) {
		s=get_product_string();
		if (s) {
			for(i=0;i<tabs;i++) {putchar('\t');}
			printf("  Product:      ");
			s->print_ascii(stdout);
			putchar('\n');
		}
	}
	if (descriptor.iSerialNumber) {
		s=get_serial_string();
		if (s) {
			for(i=0;i<tabs;i++) {putchar('\t');}
			printf("  Serial:       ");
			s->print_ascii(stdout);
			putchar('\n');
		}
	}
	for(i=0;i<descriptor.bNumConfigurations;i++) {
		if (configurations[i]) {configurations[i]->print(tabs+1);}
	}
}

void USBDevice::add_string(USBString* string) {
	__u8 index=string->get_index();
	__u16 languageId=string->get_languageId();
	if (index||languageId) add_language(languageId);
	if (index>maxStringIdx) {
		USBString*** newStrings=(USBString ***)calloc(index+1,sizeof(*newStrings));
		if (strings) {
			memcpy(newStrings,strings,sizeof(*newStrings)*(maxStringIdx+1));
			free(strings);
		}
		strings=newStrings;
		maxStringIdx=index;
	}
	int x;
	if (strings[index]) {
		int i=0;
		while (true) {
			if (strings[index][i]) {
				if (strings[index][i]->get_languageId()==languageId) {
					delete(strings[index][i]);
					strings[index][i]=string;
				}
			} else {
				strings[index]=(USBString**)realloc(strings[index],sizeof(USBString*)*(i+2));
				strings[index][i]=string;
				strings[index][i+1]=NULL;
				break;
			}
			i++;
		}
	} else {
		strings[index]=(USBString**)malloc(sizeof(USBString*)*2);
		strings[index][0]=string;
		strings[index][1]=NULL;
	}
}

//adds via proxy
void USBDevice::add_string(__u8 index,__u16 languageId) {
	if (!proxy) {fprintf(stderr,"Can't automatically add string, no device proxy defined.\n");return;}
	add_string(new USBString(proxy,index,languageId));
}

//adds for all languages
void USBDevice::add_string(__u8 index) {
	if (!strings[0]) {return;}
	if (!strings[0][0]) {return;}
	const __u16* p=strings[0][0]->get_descriptor();
	__u8 length=(p[0]&0xff)>>1;
	int i;
	for(i=1;i<length;i++) {
		add_string(index,p[i]);
	}
}

USBString* USBDevice::get_string(__u8 index,__u16 languageId) {
	if (!strings[index]) {return NULL;}
	if (!languageId&&index) {languageId=strings[0][0]->get_descriptor()[1];}
	int i=0;
	i=0;
	while (true) {
		if (strings[index][i]) {
			if (strings[index][i]->get_languageId()==languageId) {
				return strings[index][i];
			}
		} else {
			return NULL;
		}
		i++;
	}
	return NULL;
}

USBString* USBDevice::get_manufacturer_string(__u16 languageId) {
	if (!descriptor.iManufacturer) {return NULL;}
	return get_string(descriptor.iManufacturer,languageId?languageId:strings[0][0]->get_descriptor()[1]);
}

USBString* USBDevice::get_product_string(__u16 languageId) {
	if (!descriptor.iProduct) {return NULL;}
	return get_string(descriptor.iProduct,languageId?languageId:strings[0][0]->get_descriptor()[1]);
}

USBString* USBDevice::get_serial_string(__u16 languageId) {
	if (!descriptor.iSerialNumber) {return NULL;}
	return get_string(descriptor.iSerialNumber,languageId?languageId:strings[0][0]->get_descriptor()[1]);
}

void USBDevice::add_language(__u16 languageId) {
	int count=get_language_count();
	int i;
	const __u16* list=strings[0][0]->get_descriptor();
	for (i=1;i<(count+1);i++) {
		if (languageId==list[i]) {return;}
	}
	strings[0][0]->append_char(languageId);
}

__u16 USBDevice::get_language_by_index(__u8 index) {
	if (index>=get_language_count()) {return 0;}
	return strings[0][0]->get_descriptor()[index+1];
}

int USBDevice::get_language_count() {
	return strings[0][0]->get_char_count();
}

