/*
 * 573in1 - Copyright (C) 2022-2024 spicyjpeg
 *
 * 573in1 is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * 573in1 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * 573in1. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "common/blkdev/device.hpp"
#include "common/blkdev/idebase.hpp"
#include "common/util/templates.hpp"

namespace blkdev {

/* ATAPI (SCSI) command definitions */

enum ATAPICommand : uint8_t {
	ATAPI_TEST_UNIT_READY  = 0x00,
	ATAPI_REQUEST_SENSE    = 0x03,
	ATAPI_INQUIRY          = 0x12,
	ATAPI_START_STOP_UNIT  = 0x1b,
	ATAPI_PREVENT_REMOVAL  = 0x1e,
	ATAPI_READ_CAPACITY    = 0x25,
	ATAPI_READ10           = 0x28,
	ATAPI_SEEK             = 0x2b,
	ATAPI_READ_SUBCHANNEL  = 0x42,
	ATAPI_READ_TOC         = 0x43,
	ATAPI_READ_HEADER      = 0x44,
	ATAPI_PLAY_AUDIO       = 0x45,
	ATAPI_PLAY_AUDIO_MSF   = 0x47,
	ATAPI_PAUSE_RESUME     = 0x4b,
	ATAPI_STOP             = 0x4e,
	ATAPI_MODE_SELECT      = 0x55,
	ATAPI_MODE_SENSE       = 0x5a,
	ATAPI_LOAD_UNLOAD_CD   = 0xa6,
	ATAPI_READ12           = 0xa8,
	ATAPI_READ_CD_MSF      = 0xb9,
	ATAPI_SCAN             = 0xba,
	ATAPI_SET_CD_SPEED     = 0xbb,
	ATAPI_MECHANISM_STATUS = 0xbd,
	ATAPI_READ_CD          = 0xbe
};

enum ATAPIModePage : uint8_t {
	MODE_PAGE_ERROR_RECOVERY     = 0x01,
	MODE_PAGE_CDROM              = 0x0d,
	MODE_PAGE_CDROM_AUDIO        = 0x0e,
	MODE_PAGE_CDROM_CAPABILITIES = 0x2a,
	MODE_PAGE_ALL                = 0x3f
};

enum ATAPIModePageType : uint8_t {
	MODE_PAGE_TYPE_CURRENT    = 0,
	MODE_PAGE_TYPE_CHANGEABLE = 1,
	MODE_PAGE_TYPE_DEFAULT    = 2,
	MODE_PAGE_TYPE_SAVED      = 3
};

enum ATAPIStartStopMode : uint8_t {
	START_STOP_MODE_STOP_SPINDLE  = 0,
	START_STOP_MODE_START_SPINDLE = 1,
	START_STOP_MODE_OPEN_TRAY     = 2,
	START_STOP_MODE_CLOSE_TRAY    = 3
};

/* ATAPI sense keys */

enum ATAPISenseKey : uint8_t {
	SENSE_KEY_NO_SENSE        = 0x0,
	SENSE_KEY_RECOVERED_ERROR = 0x1,
	SENSE_KEY_NOT_READY       = 0x2,
	SENSE_KEY_MEDIUM_ERROR    = 0x3,
	SENSE_KEY_HARDWARE_ERROR  = 0x4,
	SENSE_KEY_ILLEGAL_REQUEST = 0x5,
	SENSE_KEY_UNIT_ATTENTION  = 0x6,
	SENSE_KEY_DATA_PROTECT    = 0x7,
	SENSE_KEY_BLANK_CHECK     = 0x8,
	SENSE_KEY_ABORTED_COMMAND = 0xb,
	SENSE_KEY_MISCOMPARE      = 0xe
};

enum ATAPISenseQualifier : uint16_t {
	ASC_NO_SENSE_INFO          = util::concat2(0x00, 0x00), // "NO ADDITIONAL SENSE INFORMATION"
	ASC_PLAY_IN_PROGRESS       = util::concat2(0x00, 0x11), // "PLAY OPERATION IN PROGRESS"
	ASC_PLAY_PAUSED            = util::concat2(0x00, 0x12), // "PLAY OPERATION PAUSED"
	ASC_PLAY_COMPLETED         = util::concat2(0x00, 0x13), // "PLAY OPERATION SUCCESSFULLY COMPLETED"
	ASC_PLAY_ERROR             = util::concat2(0x00, 0x14), // "PLAY OPERATION STOPPED DUE TO ERROR"
	ASC_NO_AUDIO_STATUS        = util::concat2(0x00, 0x15), // "NO CURRENT AUDIO STATUS TO RETURN"
	ASC_MECHANICAL_ERROR       = util::concat2(0x01, 0x00), // "MECHANICAL POSITIONING OR CHANGER ERROR"
	ASC_NO_SEEK_COMPLETE       = util::concat2(0x02, 0x00), // "NO SEEK COMPLETE"
	ASC_NOT_READY              = util::concat2(0x04, 0x00), // "LOGICAL DRIVE NOT READY - CAUSE NOT REPORTABLE"
	ASC_NOT_READY_IN_PROGRESS  = util::concat2(0x04, 0x01), // "LOGICAL DRIVE NOT READY - IN PROGRESS OF BECOMING READY"
	ASC_NOT_READY_INIT_REQ     = util::concat2(0x04, 0x02), // "LOGICAL DRIVE NOT READY - INITIALIZING COMMAND REQUIRED"
	ASC_NOT_READY_MANUAL_REQ   = util::concat2(0x04, 0x03), // "LOGICAL DRIVE NOT READY - MANUAL INTERVENTION REQUIRED"
	ASC_LOAD_EJECT_FAILED      = util::concat2(0x05, 0x01), // "MEDIA LOAD - EJECT FAILED"
	ASC_NO_REFERENCE_POSITION  = util::concat2(0x06, 0x00), // "NO REFERENCE POSITION FOUND"
	ASC_TRACK_FOLLOW_ERROR     = util::concat2(0x09, 0x00), // "TRACK FOLLOWING ERROR"
	ASC_TRACK_SERVO_FAILURE    = util::concat2(0x09, 0x01), // "TRACKING SERVO FAILURE"
	ASC_FOCUS_SERVO_FAILURE    = util::concat2(0x09, 0x02), // "FOCUS SERVO FAILURE"
	ASC_SPINDLE_SERVO_FAILURE  = util::concat2(0x09, 0x03), // "SPINDLE SERVO FAILURE"
	ASC_UNRECOVERED_READ_ERROR = util::concat2(0x11, 0x00), // "UNRECOVERED READ ERROR"
	ASC_CIRC_UNRECOVERED_ERROR = util::concat2(0x11, 0x06), // "CIRC UNRECOVERED ERROR"
	ASC_POSITIONING_ERROR      = util::concat2(0x15, 0x00), // "RANDOM POSITIONING ERROR"
	ASC_MECHANICAL_ERROR_2     = util::concat2(0x15, 0x01), // "MECHANICAL POSITIONING OR CHANGER ERROR"
	ASC_POSITIONING_ERROR_2    = util::concat2(0x15, 0x02), // "POSITIONING ERROR DETECTED BY READ OF MEDIUM"
	ASC_REC_DATA_NO_ECC        = util::concat2(0x17, 0x00), // "RECOVERED DATA WITH NO ERROR CORRECTION APPLIED"
	ASC_REC_DATA_RETRIES       = util::concat2(0x17, 0x01), // "RECOVERED DATA WITH RETRIES"
	ASC_REC_DATA_POS_OFFSET    = util::concat2(0x17, 0x02), // "RECOVERED DATA WITH POSITIVE HEAD OFFSET"
	ASC_REC_DATA_NEG_OFFSET    = util::concat2(0x17, 0x03), // "RECOVERED DATA WITH NEGATIVE HEAD OFFSET"
	ASC_REC_DATA_RETRIES_CIRC  = util::concat2(0x17, 0x04), // "RECOVERED DATA WITH RETRIES AND/OR CIRC APPLIED"
	ASC_REC_DATA_PREV_SECTOR   = util::concat2(0x17, 0x05), // "RECOVERED DATA USING PREVIOUS SECTOR ID"
	ASC_REC_DATA_ECC           = util::concat2(0x18, 0x00), // "RECOVERED DATA WITH ERROR CORRECTION APPLIED"
	ASC_REC_DATA_ECC_RETRIES   = util::concat2(0x18, 0x01), // "RECOVERED DATA WITH ERROR CORRECTION & RETRIES APPLIED"
	ASC_REC_DATA_REALLOCATED   = util::concat2(0x18, 0x02), // "RECOVERED DATA - THE DATA WAS AUTO-REALLOCATED"
	ASC_REC_DATA_CIRC          = util::concat2(0x18, 0x03), // "RECOVERED DATA WITH CIRC"
	ASC_REC_DATA_L_EC          = util::concat2(0x18, 0x04), // "RECOVERED DATA WITH L-EC"
	ASC_PARAM_LENGTH_ERROR     = util::concat2(0x1a, 0x00), // "PARAMETER LIST LENGTH ERROR"
	ASC_INVALID_COMMAND        = util::concat2(0x20, 0x00), // "INVALID COMMAND OPERATION CODE"
	ASC_LBA_OUT_OF_RANGE       = util::concat2(0x21, 0x00), // "LOGICAL BLOCK ADDRESS OUT OF RANGE"
	ASC_INVALID_PACKET_FIELD   = util::concat2(0x24, 0x00), // "INVALID FIELD IN COMMAND PACKET"
	ASC_INVALID_PARAM_FIELD    = util::concat2(0x26, 0x00), // "INVALID FIELD IN PARAMETER LIST"
	ASC_PARAM_NOT_SUPPORTED    = util::concat2(0x26, 0x01), // "PARAMETER NOT SUPPORTED"
	ASC_PARAM_VALUE_INVALID    = util::concat2(0x26, 0x02), // "PARAMETER VALUE INVALID"
	ASC_NOT_READY_TO_READY     = util::concat2(0x28, 0x00), // "NOT READY TO READY TRANSITION, MEDIUM MAY HAVE CHANGED"
	ASC_RESET_OCCURRED         = util::concat2(0x29, 0x00), // "POWER ON, RESET OR BUS DEVICE RESET OCCURRED"
	ASC_PARAMS_CHANGED         = util::concat2(0x2a, 0x00), // "PARAMETERS CHANGED"
	ASC_MODE_PARAMS_CHANGED    = util::concat2(0x2a, 0x01), // "MODE PARAMETERS CHANGED"
	ASC_INCOMPATIBLE_MEDIUM    = util::concat2(0x30, 0x00), // "INCOMPATIBLE MEDIUM INSTALLED"
	ASC_UNKNOWN_FORMAT         = util::concat2(0x30, 0x01), // "CANNOT READ MEDIUM - UNKNOWN FORMAT"
	ASC_INCOMPATIBLE_FORMAT    = util::concat2(0x30, 0x02), // "CANNOT READ MEDIUM - INCOMPATIBLE FORMAT"
	ASC_SAVING_NOT_SUPPORTED   = util::concat2(0x39, 0x00), // "SAVING PARAMETERS NOT SUPPORTED"
	ASC_MEDIUM_NOT_PRESENT     = util::concat2(0x3a, 0x00), // "MEDIUM NOT PRESENT"
	ASC_CONDITIONS_CHANGED     = util::concat2(0x3f, 0x00), // "ATAPI CD-ROM DRIVE OPERATING CONDITIONS HAVE CHANGED"
	ASC_MICROCODE_CHANGED      = util::concat2(0x3f, 0x01), // "MICROCODE HAS BEEN CHANGED"
	ASC_INTERNAL_DRIVE_FAILURE = util::concat2(0x44, 0x00), // "INTERNAL ATAPI CD-ROM DRIVE FAILURE"
	ASC_OVERLAP_ATTEMPTED      = util::concat2(0x4e, 0x00), // "OVERLAPPED COMMANDS ATTEMPTED"
	ASC_LOAD_EJECT_FAILED_2    = util::concat2(0x53, 0x00), // "MEDIA LOAD OR EJECT FAILED"
	ASC_REMOVAL_PREVENTED      = util::concat2(0x53, 0x02), // "MEDIUM REMOVAL PREVENTED"
	ASC_UNABLE_TO_RECOVER_TOC  = util::concat2(0x57, 0x00), // "UNABLE TO RECOVER TABLE OF CONTENTS"
	ASC_OPERATOR_REQUEST       = util::concat2(0x5a, 0x00), // "OPERATOR REQUEST OR STATE CHANGE INPUT (UNSPECIFIED)"
	ASC_REMOVAL_REQUEST        = util::concat2(0x5a, 0x01), // "OPERATOR MEDIUM REMOVAL REQUEST"
	ASC_END_OF_USER_AREA       = util::concat2(0x63, 0x00), // "END OF USER AREA ENCOUNTERED ON THIS TRACK"
	ASC_ILLEGAL_TRACK_MODE     = util::concat2(0x64, 0x00), // "ILLEGAL MODE FOR THIS TRACK"
	ASC_PLAY_ABORTED           = util::concat2(0xb9, 0x00), // "PLAY OPERATION ABORTED"
	ASC_LOSS_OF_STREAMING      = util::concat2(0xbf, 0x00)  // "LOSS OF STREAMING"
};

/* Data structures */

class alignas(uint32_t) ATAPISenseData {
public:
	uint8_t errorCode;              // 0
	uint8_t _reserved;              // 1
	uint8_t senseKey;               // 2
	uint8_t info[4];                // 3-6
	uint8_t additionalLength;       // 7
	uint8_t commandSpecificInfo[4]; // 8-11
	uint8_t asc;                    // 12
	uint8_t ascQualifier;           // 13
	uint8_t unitCode;               // 14
	uint8_t senseKeySpecificHeader; // 15
	uint8_t senseKeySpecific[2];    // 16-17

	inline uint32_t getErrorLBA(void) const {
		return util::concat4(info[3], info[2], info[1], info[0]);
	}
	inline uint16_t getPackedASC(void) const {
		return util::concat2(asc, ascQualifier);
	}
};

class alignas(uint32_t) ATAPIPacket {
public:
	uint8_t command;
	uint8_t param[11];
	uint8_t _reserved[4];

	inline void setTestUnitReady(void) {
		util::clear(*this);
	}
	inline void setRequestSense(uint8_t additionalLength = 0) {
		util::clear(*this);

		command  = ATAPI_REQUEST_SENSE;
		param[3] = sizeof(ATAPISenseData) + additionalLength;
	}
	inline void setStartStopUnit(ATAPIStartStopMode mode) {
		util::clear(*this);

		command  = ATAPI_START_STOP_UNIT;
		param[3] = mode;
	}
	inline void setModeSense(
		ATAPIModePage page, size_t length,
		ATAPIModePageType type = MODE_PAGE_TYPE_CURRENT
	) {
		util::clear(*this);

		command  = ATAPI_MODE_SENSE;
		param[1] = (page & 0x3f) | (type << 6);
		param[6] = (length >> 8) & 0xff;
		param[7] = (length >> 0) & 0xff;
	}
	inline void setRead(uint32_t lba, size_t count) {
		util::clear(*this);

		command  = ATAPI_READ12;
		param[1] = (lba >> 24) & 0xff;
		param[2] = (lba >> 16) & 0xff;
		param[3] = (lba >>  8) & 0xff;
		param[4] = (lba >>  0) & 0xff;
		param[5] = (count >> 24) & 0xff;
		param[6] = (count >> 16) & 0xff;
		param[7] = (count >>  8) & 0xff;
		param[8] = (count >>  0) & 0xff;
	}
	inline void setSetCDSpeed(uint16_t value) {
		util::clear(*this);

		command  = ATAPI_SET_CD_SPEED;
		param[1] = (value >> 8) & 0xff;
		param[2] = (value >> 0) & 0xff;
	}
};

/* ATAPI block device class */

class ATAPIDevice : public IDEDevice {
private:
	inline size_t _getPacketLength(void) const {
		return (flags & REQUIRES_EXT_PACKET) ? 16 : 12;
	}

	DeviceError _requestSense(void);
	DeviceError _issuePacket(const ATAPIPacket &packet, size_t dataLength = 0);

public:
	ATAPISenseData lastSenseData;

	inline ATAPIDevice(int index)
	: IDEDevice(index) {}

	DeviceError enumerate(void);
	DeviceError poll(void);
	void handleInterrupt(void);

	DeviceError read(void *data, uint64_t lba, size_t count);

	DeviceError goIdle(bool standby = false);
	DeviceError eject(bool close = false);
};

}
