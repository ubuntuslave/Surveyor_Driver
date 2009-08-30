/*
 * surveyor_comms.h
 *
 * Functions for communicating with a surveyor SR-1 robot
 * with the XBee Pro
 *
 * Copyright (C) 2007 -  Michael Janssen (original author)
 * Copyright (C) 2009 -  Carlos Jaramillo (current maintainer)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef SURVEYOR_COMMS_H_
#define SURVEYOR_COMMS_H_

// CARLOS: comments this out for cpp:
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <limits.h>

// CARLOS: added libraries when using cpp:
//#include <sstream>

#define SRV1_IMAGE_OFF 'Z'

#define SRV1_IMAGE_SMALL 'a'
#define SRV1_IMAGE_MED 'b'
#define SRV1_IMAGE_BIG 'c'

#define SRV1_MAX_VEL_X 0.315
#define SRV1_MAX_VEL_W 2.69

#define SRV1_AXLE_LENGTH            0.258

#define SRV1_DIAMETER 0.10

/**
 * @brief Type definition that is used in the communication link between the Surveyor Driver implementation and the robot itself
 * @ingroup driver_surveyor
 */
typedef struct {

   char port[PATH_MAX]; ///< Serial port communicating on.
   int fd; ///< fd if port is open. (-1 = not valid)

   double vx; ///< velocity in the x direction
   double va; ///< angular velocity

   unsigned char need_ir; ///< Do we need to read the IR?
   int bouncedir[4]; ///< 0 = front, 1 = left, 2 = back, 3 = right

   unsigned char image_mode; ///< Mode we want images in.
   unsigned char set_image_mode; ///< Mode that the camera is set to.
   uint32_t frame_size; ///< size of JPEG frame
   char *frame; ///< Frame that holds the actual image

} srv1_comm_t;

/*
 * Creates a srv1 for use
 */
srv1_comm_t *srv1_create(const char *port);

/*
 * Destroys.
 */
void srv1_destroy(srv1_comm_t *x);

/*
 * Initialized a srv1, connecting and checking the version
 */
int srv1_init(srv1_comm_t *x);

/*
 * Sets the speed.  Forward velocity trumps rotational velocity.
 *
 * This function sets the speed for 2.5 seconds, so there is
 * an automatic failsafe if the robot drives out of range.
 *
 * \param dx Speed in meters per second forward velocity
 * \param dw Speed in radians per second rotational velocity
 * \return 1 for success, 0 for failure.
 */
int srv1_set_speed(srv1_comm_t *x, double dx, double dw);

/*
 * Attempts to read the sensors that are enabled.
 * Currently only reads images and bounced IR data.
 * \param x robot structure.
 * \return 1 for success, 0 for failure.
 */
int srv1_read_sensors(srv1_comm_t *x);

/*
 * Resets communication buffers by reading all data waiting
 * and querying the version once again.
 *
 * \param x robot structure
 * \return 1 for success, 0 for failure.
 */
int srv1_reset_comms(srv1_comm_t *x);

/*
 * Returns an approximation of the distance an object is away with
 * a IR range reading.
 *
 * \param rangereading Reading from a IR.
 * \return approximated distance in cm
 */
//double srv1_range_to_distance(int rangereading);
//
//// Added method for testing Picture Delay issue:
//int saveNamedData(const char *name, char *data, int size);
///// Save the frame
///// @arg aPrefix is the string prefix to name the image.
///// @arg aWidth is the number of 0s to pad the image numbering with.
//void savePhoto(const std::string aPrefix, char *data, int size, uint32_t aWidth = 4);

// CARLOS: comments this out for cpp:
#ifdef __cplusplus
}
#endif

#endif /* SURVEYOR_COMMS_H_ */
