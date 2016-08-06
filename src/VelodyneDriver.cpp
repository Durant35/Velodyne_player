/**************************************************************************
 *
 * Velodyne Ladar Header File
 *
 * This is the header for the velodyne ladar interface drivers.
 *
 ***************************************************************************/
#include "StdAfx.h"
#include "VelodyneDriver.h"

VelodyneDriver::VelodyneDriver(){
	assert(sizeof(velodyne_packet_t) == VELODYNE_MSG_BUFFER_SIZE);
	this->lastRotation = -1;
}

VelodyneDriver::~VelodyneDriver(){

}

/*
 *	ͨ����ͷ0xEEFF���packet��Ч��
 */
int VelodyneDriver::checkPacket(velodyne_packet_t& packet){
	if (packet.blocks[0].start_identifier == 0xEEFF){
		return 0;
	}
	else{
		printf("check packet error, start identifier: %04X\n", packet.blocks[0].start_identifier);
		return -1;
	}
}
/*
 *	�ж��Ƿ����µ�һ�����ݣ���������һ�ο�ʼ�Ƕ� lastRotation
 *	�����״�ÿɨ����һ�ܣ�packet��Ŀ��������Ҫ�� ��������һ�ܵļ������ݱ������������У����к����������
 */
bool VelodyneDriver::isNewScan(velodyne_packet_t& packet){
	// ���ڲ�������
	//#define _PRINT_AZIMUTH
#ifdef _PRINT_AZIMUTH
	static int counter = 0;
	counter++;
	printf("PKT#%d\n", counter);
	for (int i = 0; i < VELODYNE_NUM_BLOCKS_IN_ONE_PKT; i++){
		printf("rotational_pos = %d", packet.blocks[i].rotational_pos);
		if(i==0){
			printf("	lastRotation = %d", lastRotation);
		}
		printf("\n");
	}
	printf("\n\n");
#endif
	// ���packet����Ч��
	if (checkPacket(packet) != 0){
		return false;
	}
	// ʹ�õ�һ��packet����ʼ���ǳ�ʼ��lastRotation����Ϊ�������ݲɼ�����ʼ����
	if (this->lastRotation == -1){
		lastRotation = packet.blocks[0].rotational_pos;
		//printf("lastRotation = %d\n", lastRotation);
		return false;
	}
	// ÿһ֡�ı߽��180�ȿ�ʼ�������ĺ��棬�����ܱ�֤����ǰ������
	if (packet.blocks[0].rotational_pos >= 18000 && lastRotation <= 18000){
		lastRotation = packet.blocks[0].rotational_pos;

#ifdef _PRINT_AZIMUTH
		counter = 0;
#endif

		return true;
	}
	// ������һ�ܼ���������ʼ����
	else{
		lastRotation = packet.blocks[0].rotational_pos;
		return false;
	}
}
