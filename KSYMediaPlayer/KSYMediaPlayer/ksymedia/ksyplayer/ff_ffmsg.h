/*
 * ff_ffmsg.h
 *      based on PacketQueue in ffplay.c
 *
 * This file is part of ksyPlayer.
 *
 * ksyPlayer is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * ksyPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with ksyPlayer; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef FFPLAY__FF_FFMSG_H
#define FFPLAY__FF_FFMSG_H

#define FFP_MSG_FLUSH                       0
#define FFP_MSG_ERROR                       100     /* arg1 = error */
#define FFP_MSG_GETDRMKEY                   101
#define FFP_MSG_SPEED			    102
#define FFP_MSG_PREPARED                    200
#define FFP_MSG_COMPLETED                   300
#define FFP_MSG_VIDEO_SIZE_CHANGED          400     /* arg1 = width, arg2 = height */
#define FFP_MSG_SAR_CHANGED                 401     /* arg1 = sar.num, arg2 = sar.den */
#define FFP_MSG_BUFFERING_START             500
#define FFP_MSG_BUFFERING_END               501
#define FFP_MSG_BUFFERING_UPDATE            502     /* arg1 = buffering head position in time, arg2 = minimum percent in time or bytes */
#define FFP_MSG_BUFFERING_BYTES_UPDATE      503     /* arg1 = cached data in bytes,            arg2 = high water mark */
#define FFP_MSG_BUFFERING_TIME_UPDATE       504     /* arg1 = cached duration in milliseconds, arg2 = high water mark */
#define FFP_MSG_SEEK_COMPLETE               600
#define FFP_MSG_PLAYBACK_STATE_CHANGED      700
#define FFP_MSG_DEMUX_CACHE_DATA      800
#define FFP_MSG_DECODE_CACHE_DATA      801
#define FFP_MSG_PLAY_TIME     802


#define FFP_REQ_START                       20001
#define FFP_REQ_PAUSE                       20002
#define FFP_REQ_SEEK                        20003

#define FFP_ERROR_UNKNOWN                   10000
#define FFP_ERROR_IO                        10001
#define FFP_ERROR_TIMEOUT                   10002
#define FFP_ERROR_UNSUPPORT                 10003
#define FFP_ERROR_NOFILE                    10004
#define FFP_ERROR_SEEKUNSUPPORT             10005
#define FFP_ERROR_SEEKUNREACHABLE           10006
#define FFP_ERROR_DRM                       10007
#define FFP_ERROR_MEM                       10008
#define FFP_ERROR_WRONGPARAM                10009
#define FFP_ERROR_RTMPDISCONNECT                10010
#endif
