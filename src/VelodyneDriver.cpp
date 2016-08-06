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
 *	通过包头0xEEFF检查packet有效性
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
 *	判断是否是新的一周数据，并更新上一次开始角度 lastRotation
 *	激光雷达每扫描完一周，packet数目必须满足要求 满足则将这一周的激光数据保留到缓冲区中，进行后续处理分析
 */
bool VelodyneDriver::isNewScan(velodyne_packet_t& packet){
	// 用于测试旋角
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
	// 检查packet的有效性
	if (checkPacket(packet) != 0){
		return false;
	}
	// 使用第一个packet的起始旋角初始化lastRotation，作为整个数据采集的起始旋角
	if (this->lastRotation == -1){
		lastRotation = packet.blocks[0].rotational_pos;
		//printf("lastRotation = %d\n", lastRotation);
		return false;
	}
	// 每一帧的边界从180度开始，即车的后面，这样能保证车的前方连续
	if (packet.blocks[0].rotational_pos >= 18000 && lastRotation <= 18000){
		lastRotation = packet.blocks[0].rotational_pos;

#ifdef _PRINT_AZIMUTH
		counter = 0;
#endif

		return true;
	}
	// 更新新一周激光数据起始旋角
	else{
		lastRotation = packet.blocks[0].rotational_pos;
		return false;
	}
}
