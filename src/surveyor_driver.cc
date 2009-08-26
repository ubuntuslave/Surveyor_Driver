/*
 *  Player - One Hell of a Robot Server
 *  Copyright (C) 2007 -
 *      Michael Janssen
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

#include "surveyor_driver.h"

// factory creation function
Driver* Surveyor_Init(ConfigFile *cf, int section) {
	return ((Driver *)(new Surveyor(cf, section))); 
}

// Registration
void Surveyor_Register(DriverTable *table) {
	table->AddDriver("surveyor", Surveyor_Init);
} 

/** @brief Constructor for the Surveyor driver.
 * Retrieves options from the configuration file, allocates memory for each interface
 * and then reads and adds the interfaces provided in the configuration file.
 */
Surveyor::Surveyor(ConfigFile *cf, int section) 
 : Driver(cf, section, true, PLAYER_MSGQUEUE_DEFAULT_MAXLEN) { 
	memset(&this->position_addr, 0, sizeof(player_devaddr_t));
	memset(&this->camera_addr, 0, sizeof(player_devaddr_t)); 
	memset(&this->ir_addr, 0, sizeof(player_devaddr_t));
	memset(&this->dio_addr, 0, sizeof(player_devaddr_t));

	// Create a position?
	if (cf->ReadDeviceAddr(&(this->position_addr), section, "provides",
				PLAYER_POSITION2D_CODE, -1, NULL) == 0) {
		if (this->AddInterface(this->position_addr) != 0) { 
                        PLAYER_ERROR("Could not add Position2D interface for SRV-1");
			this->SetError(-1); 
			return;
		} 
	} 

	// Create a camera? 
	if (cf->ReadDeviceAddr(&(this->camera_addr), section, "provides",
				PLAYER_CAMERA_CODE, -1, NULL) == 0) {

		const char *imagetype = cf->ReadString(section, "image_size", "320x240");
		if (imagetype[0] == '3') {
			this->setup_image_mode = SRV1_IMAGE_BIG;
		} else if (imagetype[0] == '1') {
			this->setup_image_mode = SRV1_IMAGE_MED;
		} else {
			this->setup_image_mode = SRV1_IMAGE_SMALL;
		}
		if (this->AddInterface(this->camera_addr) != 0) {
                        PLAYER_ERROR("Could not add Camera interface for SRV-1");
			this->SetError(-1);
			return;
		}
	}
	
	// TODO: Implement others?  Add here.
	
	this->portname = cf->ReadString(section, "port", "/dev/ttyUSB0");

	this->srvdev = NULL;

	       // Message for checking status:
	        printf("\nCARLOS: constructor is done!");
} 


int Surveyor::Setup() {
	this->srvdev = srv1_create(this->portname);

	if (!srv1_init(this->srvdev)) { 
		srv1_destroy(this->srvdev);
		this->srvdev = NULL; 
		PLAYER_ERROR("could not connect to SRV-1");
		return -1;
	}

	this->srvdev->image_mode = this->setup_image_mode;
	printf("CARLOS: image_mode = '%c' \n", this->srvdev->image_mode );
  // Start the device thread; spawns a new thread and executes
  // Surveyor::Main(), which contains the main loop for the driver.
	this->StartThread(); 

	// Message for checking status:
	printf("CARLOS: setup is done!");

	return 0;
}

int Surveyor::Shutdown() { 
	this->StopThread(); 

	srv1_destroy(this->srvdev);
	this->srvdev = NULL;
	return 0; 
} 

void Surveyor::Main() { 
	for (;;) { 
                printf("\nCARLOS: before Processing Messages()\n");
		this->ProcessMessages(); 
                printf("\nCARLOS: after Processing Messages()\n");

		if (!srv1_read_sensors(this->srvdev)) {
			PLAYER_ERROR("failed to retrieve sensors from SRV-1");
			srv1_destroy(this->srvdev);
			return;
		}
                printf("\nCARLOS: before Publishing()\n");

		// Update position2d data;
		player_position2d_data_t posdata; 
		memset(&posdata, 0, sizeof(posdata)); 

		posdata.vel.px = this->srvdev->vx;
		posdata.vel.pa = this->srvdev->va;

       this->Publish(this->position_addr, PLAYER_MSGTYPE_DATA, PLAYER_POSITION2D_DATA_STATE, (void*)&posdata, sizeof(posdata), NULL);
       printf("\nCARLOS: after Publishing()\n");

		// Camera data;
		player_camera_data_t camdata;
		memset(&camdata, 0, sizeof(camdata));

		switch (this->srvdev->image_mode) {
			case SRV1_IMAGE_SMALL:
				camdata.width = 80;
				camdata.height = 64;
				break;
			case SRV1_IMAGE_MED:
				camdata.width = 160;
				camdata.height = 128;
				break;
			case SRV1_IMAGE_BIG:
				camdata.width = 320;
				camdata.height = 240;
				break;
		}

		camdata.fdiv = 1;
		camdata.bpp = 24;
		camdata.format = PLAYER_CAMERA_FORMAT_RGB888;
		camdata.compression = PLAYER_CAMERA_COMPRESS_JPEG;

	   printf("Surveyor::Main(): frame_size = %d\n",  this->srvdev->frame_size);
      printf("Surveyor::Main(): image_mode = '%c'\n",  this->srvdev->image_mode);


		if (this->srvdev->image_mode != SRV1_IMAGE_OFF) {
			camdata.image_count = this->srvdev->frame_size;

			if (camdata.image == NULL) {
			   camdata.image = (uint8_t *)malloc(camdata.image_count);
		   } else {
		      camdata.image = (uint8_t *)realloc(camdata.image, camdata.image_count);
		   }
			memcpy(camdata.image, this->srvdev->frame, camdata.image_count);
         printf("\nCARLOS: before Publishing CAMERA()\n");

		}

		this->Publish(this->camera_addr, PLAYER_MSGTYPE_DATA, PLAYER_CAMERA_DATA_STATE, (void*)&camdata, sizeof(camdata), NULL);
      printf("\nCARLOS: after Publishing CAMERA()\n");

		// TODO: add other interfaces' fills.
		
		usleep(SRVMIN_CYCLE_TIME);
	} 
}

int Surveyor::ProcessMessage(QueuePointer &resp_queue, player_msghdr *hdr, void *data) {

      printf("\nCARLOS: I'm in ProcessMessage\n");

      if (Message::MatchMessage(hdr, PLAYER_MSGTYPE_CMD,
				PLAYER_POSITION2D_CMD_VEL, this->position_addr)) {
        printf("\nCARLOS: I'm Matching Message\n");

                // position motor command
		player_position2d_cmd_vel_t position_cmd;
		position_cmd = *(player_position2d_cmd_vel_t *) data;
		PLAYER_MSG2(2,"sending motor commands %f %f", position_cmd.vel.px, position_cmd.vel.pa);
		
		if (!srv1_set_speed(this->srvdev, position_cmd.vel.px, position_cmd.vel.pa)) {
			PLAYER_ERROR("failed to set speed on SRV-1");
                  }

		return 0; 
	}
      else if (Message::MatchMessage(hdr, PLAYER_MSGTYPE_REQ,
              PLAYER_POSITION2D_REQ_MOTOR_POWER,
              this->position_addr))
        {
          this->Publish(this->position_addr, resp_queue,
             PLAYER_MSGTYPE_RESP_ACK, PLAYER_POSITION2D_REQ_MOTOR_POWER);
          return 0;
        }
      else if(Message::MatchMessage(hdr,PLAYER_MSGTYPE_REQ,
                                      PLAYER_POSITION2D_REQ_GET_GEOM,
                                      this->position_addr))
        {
          /* Return the robot geometry. */
          memset(&pos_geom, 0, sizeof pos_geom);
          // Assume that it turns about its geometric center, so zeros are fine

          pos_geom.size.sl = SRV1_DIAMETER;
          pos_geom.size.sw = SRV1_DIAMETER;

          this->Publish(this->position_addr, resp_queue,
                            PLAYER_MSGTYPE_RESP_ACK,
                            PLAYER_POSITION2D_REQ_GET_GEOM,
                            (void*)&pos_geom, sizeof pos_geom, NULL);
          return 0;
        }
        else {
		return -1; 
	} 
	// No commands / requests for cameras.
}

////////////////////////////////////////////////////////////////////////////////
// Extra stuff for building a shared object.

/* need the extern to avoid C++ name-mangling  */
extern "C" {
  int player_driver_init(DriverTable* table)
  {
    puts("Surveyor driver initializing");
    Surveyor_Register(table);
    puts("Surveyor driver done");
    return(0);
  }
}
