#ifndef __VELODYNE_DRIVER_H__
#define __VELODYNE_DRIVER_H__

/**************************************************************************
 *
 * Velodyne Ladar Header File
 *
 * This is the header for the velodyne ladar interface drivers.
 *
 ***************************************************************************/
/*
 *	added by durant35
 *		#pragma pack(i)(i = 1,2,4,8,16)�����ö����ֽ���Ŀ
 *		vs����������Ŀ����-��������-c/c++-��������-�ṹ��Ա��������
 */
#pragma pack(1)

#include <stdint.h>

#define VELODYNE_NUM_BEAMS_IN_ONE_BLOCK			32
#define VELODYNE_NUM_BLOCKS_IN_ONE_PKT			12

#define PKT_NUM_IN_CIRCLE_LOWER_BOUND			180
#ifdef USE_VLP_16_
	#define PKT_NUM_IN_CIRCLE_LOWER_BOUND		75
#endif
#ifdef USE_HDL_64E_
	#define PKT_NUM_IN_CIRCLE_LOWER_BOUND		360
#endif

#define VELODYNE_UDP_PORT						2368
#define VELODYNE_PACKET_SIZE					1248
#define VELODYNE_UDP_OFFSET						42
#define VELODYNE_PACKET_WITH_PCAPHDR_SIZE		1264
#define VELODYNE_MSG_BUFFER_SIZE				1206
#define VELODYNE_UDP_PCAP_OFFSET				58

class VelodyneDriver{
public:
	/*
	 *	Velodyne data structure(1206 bytes in each packet)
	 *		12*(2[start identifier]+2[Rotational]+32*3)
	 *	  + 6(status data:4[GPS timestamp]+1[Status Byte]+1[Status Value])
	 */
	typedef struct {
		uint16_t distance;  										// 2mm increments
		uint8_t intensity;  										// 255 being brightest
	}  velodyne_fire_t;
	/*
	 *	����16�߼����״� ����ͬ���ǵ�����fire��Ϊһ��block
	 *	����64�߼����״� һ��fire��ֳ�����block
	 */
	typedef struct {
		uint16_t start_identifier;
		uint16_t rotational_pos;									// 0-35999  divide by 100 for degrees
		velodyne_fire_t lasers[VELODYNE_NUM_BEAMS_IN_ONE_BLOCK];
	}  velodyne_block_t;

	typedef struct {
		velodyne_block_t blocks[VELODYNE_NUM_BLOCKS_IN_ONE_PKT];
		uint8_t GPS_timestamp[4];
		uint8_t status_byte;
		uint8_t status_value;
	}  velodyne_packet_t;

	VelodyneDriver();
	~VelodyneDriver();

	int checkPacket(velodyne_packet_t& packet_reference);
	bool isNewScan(velodyne_packet_t& packet_reference);
private:
	// ��ʼΪ-1
	int lastRotation;
};

#endif

