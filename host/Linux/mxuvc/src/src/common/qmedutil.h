/*******************************************************************************
 *
 * The content of this file or document is CONFIDENTIAL and PROPRIETARY
 * to Maxim Integrated Products.  It is subject to the terms of a
 * License Agreement between Licensee and Maxim Integrated Products.
 * restricting among other things, the use, reproduction, distribution
 * and transfer.  Each of the embodiments, including this information and
 * any derivative work shall retain this copyright notice.
 *
 * Copyright (c) 2012 Maxim Integrated Products.
 * All rights reserved.
 *
 ******************************************************************************/
/*******************************************************************************
* @(#) $Id$
*******************************************************************************/

#ifndef QMED_UTIL_HH
#define QMED_UTIL_HH

/*** QMedBase Struct ***/

unsigned char * AllocQMedBaseStruct(int version, int extHdrSize);
void FreeQMedBaseStruct(unsigned char *pQMed);

unsigned long   GetQMedBaseBoxSize(unsigned char *pQMed);
unsigned long   GetQMedBaseBoxType(unsigned char *pQMed);
unsigned long   GetQMedBaseBoxFlags(unsigned char *pQMed);
unsigned long   GetQMedBaseVersion(unsigned char *pQMed);
unsigned long   GetQMedBaseFlags(unsigned char *pQMed);
unsigned long   GetQMedBaseMajorMediaType(unsigned char *pQMed);
unsigned long   GetQMedBaseMinorMediaType(unsigned char *pQMed);
unsigned long   GetQMedBaseHashSize(unsigned char *pQMed);
unsigned long * GetQMedBaseHashPayload(unsigned char *pQMed);

void SetQMedBaseBoxSize(unsigned char *pQMed, unsigned long size);
void SetQMedBaseBoxType(unsigned char *pQMed, unsigned long type);
void SetQMedBaseBoxFlags(unsigned char *pQMed, unsigned long flags);
void SetQMedBaseVersion(unsigned char *pQMed, unsigned char v);
void SetQMedBaseFlags(unsigned char *pQMed, unsigned long f);
void SetQMedBaseMajorMediaType(unsigned char *pQMed, unsigned long majorMediaType);
void SetQMedBaseMinorMediaType(unsigned char *pQMed, unsigned long minorMediaType);
void SetQMedBaseHashSize(unsigned char *pQMed, unsigned long hashSize);
void SetQMedBaseHashPayload(unsigned char *pQMed, unsigned long idx, unsigned long hashPayload);

/*** QMedH264 Struct ***/

#define FreeQMedH264Struct(p) FreeQMedBaseStruct(p)

unsigned char * AllocQMedH264Struct(int baseVersion, int h264Version);

unsigned long GetQMedH264HeaderSize(int baseVersion, int h264Version);
unsigned long GetQMedH264Version(unsigned char *pQMed);
unsigned long GetQMedH264Width(unsigned char *pQMed);
unsigned long GetQMedH264Height(unsigned char *pQMed);
unsigned long GetQMedH264SampleTicks(unsigned char *pQMed);
unsigned long GetQMedH264MotionCounter(unsigned char *pQMed);
unsigned long GetQMedH264MotionBitmapSize(unsigned char *pQMed);

void SetQMedH264Version(unsigned char *pQMed, unsigned long version);
void SetQMedH264Width(unsigned char *pQMed, unsigned long width);
void SetQMedH264Height(unsigned char *pQMed, unsigned long height);
void SetQMedH264SampleTicks(unsigned char *pQMed, unsigned long sampleTicks);
void SetQMedH264MotionCounter(unsigned char *pQMed, unsigned long motionCounter);
void SetQMedH264MotionBitmapSize(unsigned char *pQMed, unsigned long motionBitmapSize);

/*** QMedPCM Struct ***/

#define FreeQMedPCMStruct(p) FreeQMedBaseStruct(p)

unsigned char * AllocQMedPCMStruct(int baseVersion, int pcmVersion);

unsigned long GetQMedPCMHeaderSize(int baseVersion, int pcmVersion);
unsigned long GetQMedPCMVersion(unsigned char *pQMed);
unsigned int  GetQMedPCMSampleFrequency(unsigned char *pQMed);
unsigned int  GetQMedPCMAccessUnits(unsigned char *pQMed);
unsigned int  GetQMedPCMAccessUnitSize(unsigned char *pQMed);
unsigned int  GetQMedPCMChannels(unsigned char *pQMed);

void SetQMedPCMVersion(unsigned char *pQMed, unsigned long version);
void SetQMedPCMSampleFrequency(unsigned char *pQMed, unsigned int sampleFrequency);
void SetQMedPCMAccessUnits(unsigned char *pQMed, unsigned int accessUnits);
void SetQMedPCMAccessUnitSize(unsigned char *pQMed, unsigned int accessUnitSize);
void SetQMedPCMChannels(unsigned char *pQMed, unsigned int channels);

/*** QMedAAC Struct ***/

#define FreeQMedAACStruct(p) FreeQMedBaseStruct(p)

unsigned char * AllocQMedAACStruct(int baseVersion, int aacVersion);

unsigned long GetQMedAACHeaderSize(int baseVersion, int aacVersion);
unsigned long GetQMedAACVersion(unsigned char *pQMed);
unsigned int  GetQMedAACSampleFrequency(unsigned char *pQMed);
unsigned int  GetQMedAACChannels(unsigned char *pQMed);
unsigned int  GetQMedAACSampleSize(unsigned char *pQMed);
unsigned int  GetQMedAACAudioSpecificConfigSize(unsigned char *pQMed);

void SetQMedAACVersion(unsigned char *pQMed, unsigned long version);
void SetQMedAACSampleFrequency(unsigned char *pQMed, unsigned int sampleFrequency);
void SetQMedAACChannels(unsigned char *pQMed, unsigned int channels);
void SetQMedAACSampleSize(unsigned char *pQMed, unsigned int sampleSize);
void SetQMedAACAudioSpecificConfigSize(unsigned char *pQMed, unsigned int audioSpecificConfigSize);

/*** QMedMP2 Struct ***/

#define FreeQMedMP2Struct(p) FreeQMedBaseStruct(p)

unsigned char * AllocQMedMP2Struct(int baseVersion, int mp2Version);

unsigned long GetQMedMP2HeaderSize(int baseVersion, int mp2Version);
unsigned long GetQMedMP2Version(unsigned char *pQMed);
unsigned int  GetQMedMP2SampleFrequency(unsigned char *pQMed);
unsigned int  GetQMedMP2Channels(unsigned char *pQMed);

void SetQMedMP2Version(unsigned char *pQMed, unsigned long version);
void SetQMedMP2SampleFrequency(unsigned char *pQMed, unsigned int sampleFrequency);
void SetQMedMP2Channels(unsigned char *pQMed, unsigned int channels);

/*** QMedJPEG Struct ***/

#define FreeQMedJPEGStruct(p) FreeQMedBaseStruct(p)

unsigned char * AllocQMedJPEGStruct(int baseVersion, int jpegVersion);

unsigned long GetQMedJPEGHeaderSize(int baseVersion, int jpegVersion);
unsigned long GetQMedJPEGVersion(unsigned char *pQMed);
unsigned long GetQMedJPEGWidth(unsigned char *pQMed);
unsigned long GetQMedJPEGHeight(unsigned char *pQMed);
unsigned long GetQMedJPEGFrameTicks(unsigned char *pQMed);

void SetQMedJPEGVersion(unsigned char *pQMed, unsigned long version);
void SetQMedJPEGWidth(unsigned char *pQMed, unsigned long width);
void SetQMedJPEGHeight(unsigned char *pQMed, unsigned long height);
void SetQMedJPEGFrameTicks(unsigned char *pQMed, unsigned long sampleTicks);

/*** QMed711 Struct ***/

#define FreeQMed711Struct(p) FreeQMedBaseStruct(p)

unsigned char * AllocQMed711Struct(int baseVersion, int g711Version);

unsigned long GetQMed711HeaderSize(int baseVersion, int g711Version);
unsigned long GetQMed711Version(unsigned char *pQMed);
unsigned int  GetQMed711AccessUnits(unsigned char *pQMed);
unsigned int  GetQMed711SampleSize(unsigned char *pQMed);

void SetQMed711Version(unsigned char *pQMed, unsigned long version);
void SetQMed711AccessUnits(unsigned char *pQMed, unsigned int accessUnits);
void SetQMed711SampleSize(unsigned char *pQMed, unsigned int sampleSize);

/*** QMed722 Struct ***/

#define FreeQMed722Struct(p) FreeQMedBaseStruct(p)

unsigned char * AllocQMed722Struct(int baseVersion, int g722Version);

unsigned long GetQMed722HeaderSize(int baseVersion, int g722Version);
unsigned long GetQMed722Version(unsigned char *pQMed);
unsigned int  GetQMed722Bitrate(unsigned char *pQMed);
unsigned int  GetQMed722AccessUnits(unsigned char *pQMed);
unsigned int  GetQMed722SampleSize(unsigned char *pQMed);

void SetQMed722Version(unsigned char *pQMed, unsigned long version);
void SetQMed722Bitrate(unsigned char *pQMed, unsigned long bitrate);
void SetQMed722AccessUnits(unsigned char *pQMed, unsigned int accessUnits);
void SetQMed722SampleSize(unsigned char *pQMed, unsigned int sampleSize);

/*** QMed726 Struct ***/

#define FreeQMed726Struct(p) FreeQMedBaseStruct(p)

unsigned char * AllocQMed726Struct(int baseVersion, int g726Version);

unsigned long GetQMed726HeaderSize(int baseVersion, int g726Version);
unsigned long GetQMed726Version(unsigned char *pQMed);
unsigned int  GetQMed726Bitrate(unsigned char *pQMed);
unsigned int  GetQMed726AccessUnits(unsigned char *pQMed);
unsigned int  GetQMed726SampleSize(unsigned char *pQMed);

void SetQMed726Version(unsigned char *pQMed, unsigned long version);
void SetQMed726Bitrate(unsigned char *pQMed, unsigned long bitrate);
void SetQMed726AccessUnits(unsigned char *pQMed, unsigned int accessUnits);
void SetQMed726SampleSize(unsigned char *pQMed, unsigned int sampleSize);

/*** QMed728 Struct ***/

#define FreeQMed728Struct(p) FreeQMedBaseStruct(p)

unsigned char * AllocQMed728Struct(int baseVersion, int g728Version);

unsigned long GetQMed728HeaderSize(int baseVersion, int g728Version);
unsigned long GetQMed728Version(unsigned char *pQMed);
unsigned int  GetQMed728AccessUnits(unsigned char *pQMed);

void SetQMed728Version(unsigned char *pQMed, unsigned long version);
void SetQMed728AccessUnits(unsigned char *pQMed, unsigned int accessUnits);

#endif
