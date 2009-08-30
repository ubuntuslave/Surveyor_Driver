/*
 *  Player - One Hell of a Robot Server
 *  Copyright (C) 2007 -  Michael Janssen (original author)
 *  Copyright (C) 2009 -  Carlos Jaramillo (current maintainer)
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc., 51
 *  Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#ifndef SURVEYOR_DRIVER_H_
#define SURVEYOR_DRIVER_H_

#include <unistd.h>

#include <libplayercore/playercore.h>

#include "surveyor_comms.h"

#define SRVMIN_CYCLE_TIME 200000

/** @ingroup drivers */

/** @{ */

/** @defgroup driver_surveyor Surveyor

 @class Surveyor "src/surveyor_driver.h"

 @ingroup driver_surveyor

 @brief Plugin Driver for Surveyor SRV-1 (with ARM-7 processor)

 These robots can be controlled via ZigBee which is attached to a USB
 serial port on a host computer.  The host computer runs the server which
 then communicates to the robot.

 @note
 The newer SRV-1 robot (with Blackfin processor) has not yet been tested with this driver.

 @par  Compile-time dependencies

 - `pkg-config --cflags playerc++`

 @par  Provides

 The surveyor driver provides the following device interfaces:

 - @ref interface_position2d
 - This interface does not return odometry data, but accepts velocity commands.

 - @ref interface_camera
 - The camera on the robot returns JPEG images.

 - @ref interface_ir
 - The robot has 4 IR beacons which can act as rudimentary range-finders
 - UNIMPLEMENTED

 - @ref interface_dio
 - The robot has 5 pins which can be used as digital in/out ports.
 - UNIMPLEMENTED

 @par  Supported configuration requests

 - none

 @par  Configuration file options

 - port (string)
 - Serial port used to communicate with the robot
 - Default: "/dev/ttyUSB0"
 - image_size (string)
 - Size of the images returned by the camera.
 - Default: "320x240"
 - Allowed values: "320x240", "160x128", "80x64"
 - plugin (string)
 - Relative or Absolute path to the location of the shared-object plugin driver.

 @par  Example

 @verbatim
 driver
    (
       name "surveyor"
       plugin "libSurveyor_Driver.so"
       provides ["position2d:0" "camera:0"]
       port "/dev/ttyUSB0"
    )
 @endverbatim

 @bug
 - Camera interface has a small delay for snapshots (Robot has to focus first, and then shoot)
 - Camera rate is very slow - about 1fps...Could do better: at least 4fps

 @todo
 - Property bags to change image size on the fly
 - Implement IR (IR sensors are very noisy and produce false negatives on dark and shiny obstacles)
 - Implement DIO
 - Opaque interface to set program

 @author Michael Janssen (original author)
 @author Carlos Jaramillo (current maintainer)

 */
/** @} */

class Surveyor : public Driver
{
   public:
      /** @brief Constructor for the Surveyor multi-interface driver.
       * @param cf Current configuration file
       * @param section Current section in configuration file
       */
      Surveyor(ConfigFile *cf, int section);
      //      ~Surveyor(void);  ///< Destructor

      /** @brief Set up the device and start the device thread by calling StartThread(), which spawns a new thread and executes
       * Surveyor::Main(), which contains the main loop for the driver
       * @returns 0 if things go well, and -1 otherwise.
       */
      int
      Setup();

      /** @brief  Shut down the device
       * @returns 0 when the device has been completely shut down.
       */
      int
      Shutdown();

      /** @brief  Message handler that sends a response if necessary using Publish().
       *  This function is called once for each message in the incoming queue.
       *  @param resp_queue The queue to which any response should go
       *  @param hdr The message header
       *  @param data The message body
       *  @returns 0 if the message in handled, and -1 otherwise. A NACK (Negative Acknowledgment) will be sent if a response is required.
       */
      int
      ProcessMessage(QueuePointer & resp_queue, player_msghdr *hdr, void *data);

   private:

      /** @brief  Main "entry point" function for the driver thread created using
       * StartThread() within the Setup() function;
       */
      virtual void
      Main();

      const char *portname; ///< Serial port

      player_devaddr_t position_addr; ///< Address of the position device (wheels odometry)
      player_devaddr_t camera_addr; ///< Address of the camera device
      player_devaddr_t ir_addr; ///< Address of the infrared (IR) beacons
      player_devaddr_t dio_addr; ///< Address of the digital input/output pins (ports)

      srv1_comm_t *srvdev; ///< The surveyor object

      player_position2d_cmd_vel_t position_cmd; ///< position2d velocity command
      player_position2d_geom_t pos_geom; ///< position2d geometry

      int setup_image_mode; ///< Desired camera size
};

/** @brief Factory creation function that instantiates the Driver
 * @returns a pointer (as a generic Driver*) to a new instance of this driver
 */
Driver*
Surveyor_Init(ConfigFile *cf, int section);

/** @brief Driver registration function that adds the driver into the given driver table.
 * The startup registration is accomplished via this function, which in addition to the driver class factory function "Surveyor_Init"
 * also takes the name of the driver as specified in the server configuration file.
 *
 * @param table The table where this driver will get registered into
 */
void
Surveyor_Register(DriverTable *table);

#endif /* SURVEYOR_DRIVER_H_ */
