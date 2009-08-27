/* 
 * surveyor_comms.c
 *
 * Functions for communicating with a SRV-1 robot from Surveyor.
 *
 * Copyright (C) 2007 Michael Janssen
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
 *
 */

#include "surveyor_comms.h"

#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <math.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
// CARLOS: added libraries:
#include <iomanip>
#include <iostream>

srv1_comm_t *srv1_create(const char *port) { 
	srv1_comm_t *ret = (srv1_comm_t *) malloc(sizeof(srv1_comm_t));

	ret->fd = -1; 
	ret->image_mode = SRV1_IMAGE_OFF;
	ret->set_image_mode = SRV1_IMAGE_OFF;
	ret->need_ir = 0;

	ret->frame_size = 0;

	ret->vx = 0.0; 
	ret->va = 0.0;

	ret->frame = NULL;

	strncpy(ret->port, port, sizeof(ret->port) - 1);

	return ret;
} 

/* 
 * Flips non-blocking bit.
 */
int flip_nonblock(int fd) {
	int flags;
	if ((flags = fcntl(fd, F_GETFL)) < 0) {
		perror("surveyor_flip_nonblock():fcntl():");
		close(fd);
		return -1;
	}

	if (fcntl(fd, F_SETFL, flags ^ O_NONBLOCK) < 0) {
		perror("surveyor_flip_nonblock():fcntl():");
		close(fd);
		return -1;
	}

	return 0;
}


int32_t msecsub(struct timeval a, struct timeval b) { 
	return ((a.tv_sec - b.tv_sec) * 1000000) + (a.tv_usec - b.tv_usec);
} 

/* 
 * Reads b bytes from fd.  times out in s seconds, returning 0.
 * Buf needs to have enough space in it (non-checking)
 */
int read_limited(int fd, char *buf, int bytes, int microsecs) {
	flip_nonblock(fd);

	struct timeval begin, now;
	gettimeofday(&begin, NULL);

	int needtoread = bytes;
	int readresult;

	for (;;) {
		gettimeofday(&now, NULL);
		if (msecsub(now, begin) > microsecs) {
			printf("read_limited():Warning: CARLOS timed out (%d microsecs).\n", msecsub(now, begin));
			flip_nonblock(fd);
			return (bytes - needtoread);
		}

		if ((readresult = read(fd, buf + (bytes - needtoread), needtoread)) < 0) {
			if (errno != EAGAIN) {
				perror("read_limited():read()");
				flip_nonblock(fd);
				return -1;
			}
		} else {
			needtoread = needtoread - readresult;
			if (needtoread == 0) {
				break;
			}
		}

		usleep(20);
	}

	flip_nonblock(fd);
	return bytes;
}

/* 
 * Flushes input buffer.
 * \return number of bytes discarded.
 */ 
int srv1_flush_input(srv1_comm_t *x) {
	int res;
	ioctl(x->fd, TIOCINQ, (char *)&res);
	tcflush(x->fd, TCIFLUSH);
	return res;
} 

int srv1_open(srv1_comm_t *x) { 

	struct termios term;
//	int flags;
	int fd; 

	printf("Opening connection to Surveyor on %s...", x->port);

	if ((fd = open(x->port, O_RDWR | O_NONBLOCK, S_IRUSR, S_IWUSR)) < 0) {
		perror("surveyor_open():open():");
		return 0;
	}

	if (tcflush(fd, TCIFLUSH) < 0) {
		perror("surveyor_open():tcflush():");
		close(fd);
		return 0;
	}

	if (tcgetattr(fd, &term) < 0) {
		perror("surveyor_open():tcgetattr():");
		close(fd);
		return 0;
	}

	cfmakeraw(&term);
	cfsetispeed(&term, B115200);
	cfsetospeed(&term, B115200);

	if (tcsetattr(fd, TCSAFLUSH, &term) < 0) {
		perror("surveyor_open():tcsetattr():");
		close(fd);
		return 0; 
	}

	/* Let's set blocking for now. */
	flip_nonblock(fd);

	puts("Done.");

	x->fd = fd;

	return 1;
}

void srv1_close(srv1_comm_t *x) {
        printf("\nCARLOS: now closing srv1_close()\n");
	srv1_set_speed(x, 0, 0);
	
	close(x->fd);
} 

void srv1_destroy(srv1_comm_t *x) { 
	if (x->frame != NULL) { 
		free(x->frame);
	} 

	if (x->fd != -1) {
		srv1_close(x);
	} 

	free(x);
	return;
} 

int srv1_init(srv1_comm_t *x) { 
	assert(x);

	if (srv1_open(x) == 0) {
		// Error. srv1_open handles half-open closing.
		return 0;
	} 

	// Check to see that we can communicate by sending a #V
	char buf[256];
	if (write(x->fd, "V", 1) < 0) {
		printf("srv1_init(): can't write to port %d!\n", x->port);
		return 0;
	} 

	int spot = 0;
	memset(buf, 0, 256);
//	while(buf[spot-1] != '\n') { // CARLOS: Changed the array subscript that was below array bounds
//		int num = read(x->fd, buf + spot, 1);
//		if (num != 1) {
//			printf("srv1_init(): Can't read a byte from surveyor!\n");
//			return 0;
//		}
//		spot++;
//	}

//	Compiled before:
//         do { // CARLOS: Changed the array subscript that was below array bounds
//                  int num = read(x->fd, buf + spot, 1);
//                  if (num != 1) {
//                          printf("srv1_init(): Can't read a byte from surveyor!\n");
//                          return 0;
//                  }
//                  spot++;
//           }
//         while(buf[spot-1] != '\n');

// Original:
         while(buf[spot-1] != '\n') {
                  int num = read(x->fd, buf + spot, 1);
                  if (num != 1) {
                          printf("srv1_init(): Can't read a byte from surveyor!\n");
                          return 0;
                  }
                  spot++;
          }
	// Print the version number
	printf("srv1_init(): successful init. HW %s", buf + 2);

	return 1;
}

double calc_forward(signed char speed) {
	unsigned char speedg = (speed > 0 ? speed : -speed);
	if (speedg < 20) {
		return 0;
	} 
	double i3 = 5.5277e-7;
	double j3 = -0.00016261;
	double k3 = 0.01628;
	double l3 = -0.26129;
	// third order is ultra-small... probably second-order.
	double x = k3 * speedg;
	double y = j3 * speedg * speedg;
	double z = i3 * speedg * speedg * speedg;
	double result = l3 + x + y + z;
	printf("calc_forward(%d) = %f + %f + %f + %f = %f\n", speed, l3, x, y, z, result);
	return (speed > 0 ? result : -result);
} 

signed char calc_speed_hackish(double dx) { 
	if (fabs(dx) > SRV1_MAX_VEL_X) {
		printf("srv1_set_speed(): warning: speed out of range.\n");
		return (dx > 0.0 ? 127 : -127);
	}

	if (fabs(dx) < 0.05) {
		return 0;
	} 

	signed char current = 20;

	for (;;) {
		current = current + 1;
		if (calc_forward(current) > fabs(dx)) { 
			return (dx < 0.0 ? -(current - 1) : current - 1);
		} 
	} 
}

double calc_angular(int left, int right) {
	double basis = 0.234;
	printf("calc_angular(%d, %d) = %f\n", left, right, (calc_forward(right) - calc_forward(left)) / basis);
	return (calc_forward(right) - calc_forward(left)) / basis;
}

void calc_rot_hackish(double dx, signed char *left, signed char *right) {
	int direction = 0;
	int l = *left; 
	int r = *right;
	double angular = 0;
//	double newangular;
	if (fabs(dx) < 0.05) {
		return;
	} 
	// If not moving, move both
	if (l == 0) { 
		while ((l > -127) && (l < 127) 
				&& (r > -127) && (r < 127)) { 
			angular = calc_angular(l, r);
			if (angular < dx) { 
				if (direction == -1) { 
					break;
				}
				if (r < 20) {
					r = 20;
					l = -r;
				} 
				// left back, right forward.
				l--; r++;
				direction = 1;
			} 
			if (angular > dx) {
				if (direction == 1) { 
					break;
				} 
				if (l < 20) {
					l = 20;
					r = -l;
				} 
				l++; r--;
				direction = -1;
			}
		}
	// If going forward, shift up to attempt rotation
	} else if (l > 0) { 
		while ((l < 127) && (r < 127)) {
			angular = calc_angular(l, r);
			if (angular < dx) { 
				if (direction == -1) {
					break;
				} 
				// Right faster
				r++;
				direction = 1;
			}
			if (angular > dx) { 
				if (direction == 1) {
					break; 
				}
				l++;
				direction = -1;
			} 
		}
	// If going backward, shift down to attempt rotation
	} else { 
		while ((l > -127) && (r > -127)) {
			angular = calc_angular(l, r);
			if (angular < dx) {
				if (direction == -1) {
					break;
				}
				// Left slower = backwards faster
				l--;
				direction = 1;
			} 
			if (angular > dx) {
				if (direction == 1) {
					break;
				} 
				r--;
				direction = -1;
			} 
		}
	}
	
	if (((r == 127) || (r == -127) || (l == 127) || (l == -127))
			&& (fabs(dx - calc_angular(l, r)) > 0.01)) {
		printf("srv1_set_speed(): warning: can't achieve %f rotation. got %f.\n",
				dx, calc_angular(l, r));
	}

	*right = r; 
	*left = l;
} 

int srv1_set_motors(srv1_comm_t *x, signed char l, signed char r, double t) {
	char cmdbuf[4];
//	float timeunits;
	char runtime;

//	timeunits = t * 100;
//	if (timeunits > 255) {
//		runtime = 255;
//	} else if (timeunits < 0) {
//		runtime = 0;
//	} else {
//		runtime = timeunits;
//	}
	
	runtime = 0;  // CARLOS: forcing an indefinite duration

	// Command:    'Mabc'
        //	direct motor control
        //	'abc' parameters sent as 8-bit binary
        //
        //	a=left speed, b=right speed, c=duration*10milliseconds
        //
        //	speeds are 2's complement 8-bit binary values
	//             - 0x00 through 0x7F is forward,
	//             0xFF through 0x81 is reverse,
        //      e.g. the decimal equivalent of the 4-byte sequence 0x4D 0x32 0xCE 0x14 = 'M' 50 -50 20 (rotate right for 200ms)
        //
        //	duration of 00 is infinite, e.g. the 4-byte sequence 0x4D 0x32 0x32 0x00 = M 50 50 00 (drive forward at 50% indefinitely)
	cmdbuf[0] = 'M';
	cmdbuf[1] = l;
	cmdbuf[2] = r;
	cmdbuf[3] = runtime;

	if (write(x->fd, cmdbuf, 4) < 0) {
		// TODO: do something useful
//		return 0;   // CARLOS: thinks this should be commented this out here
	} 

	// Response:   '#M'
	if (read_limited(x->fd, cmdbuf, 2, 250000) == 2) {
		if (cmdbuf[0] == '#' &&
				cmdbuf[1] == 'M') {
			return 1;
		}
		printf("srv1_set_speed(): warning: failed response: %c%c!!!\n", cmdbuf[0], cmdbuf[1]);
//		int bytes = srv1_flush_input(x);
		printf("srv1: flushed %d bytes from input buffer..\n");
//		return 0;   // CARLOS: thinks this should be commented this out here
	}

	return 0;
} 

int srv1_set_speed(srv1_comm_t *x, double dx, double dw) { 

	signed char leftspeed;
	signed char rightspeed;

	printf("srv1_set_speed(): debug: dx: %2.2f dw: %2.2f\n", dx, dw);
	leftspeed = calc_speed_hackish(dx); 

	rightspeed = leftspeed;
	
	x->vx = calc_forward(leftspeed);

	calc_rot_hackish(dw, &leftspeed, &rightspeed);

	x->va = calc_angular(leftspeed, rightspeed);

	// The SRV-1 speed gets actually set here
        // Moving the motors for an indefinitely amount of time (0.0)
	return srv1_set_motors(x, leftspeed, rightspeed, 0.0);
}


int srv1_fill_image(srv1_comm_t *x) {
	
   // CARLOS: pause for debugging and see what picture should be taking:
   std::cin.ignore();

	char specbuf[10];

	memset(specbuf, 0, 10);

	if (x->set_image_mode != x->image_mode) {	
		if (x->image_mode != SRV1_IMAGE_OFF) {
			printf("srv1_fill_image(): setting image mode '%c'\n", x->image_mode);
			if (write(x->fd, &(x->image_mode), 1) < 0) { 
				return 0; 
			} 

			int done = read_limited(x->fd, specbuf, 2, 500000);
			
			if (done != 2) {
				// TODO: do something more important
				int btsdead = srv1_flush_input(x);
				printf("srv1_fill_image(): discarded %d bytes.\n", btsdead);
				return 0;
			}

			if (specbuf[0] != '#' || specbuf[1] != x->image_mode) {
				printf("srv1_fill_image(): didn't get correct response from image size set: %s\n", specbuf);
				int btsdead = srv1_flush_input(x);
				printf("srv1_fill_image(): discarded %d bytes.\n", btsdead);
				return 0;
			} else {
				x->set_image_mode = x->image_mode;
			}
		} else {
			x->set_image_mode = x->image_mode;
		} 
	}

	if (x->set_image_mode == SRV1_IMAGE_OFF) {
		return 1;
	} 
	printf("srv1_fill_image(): Image Mode '%c'\n", x->set_image_mode);
	int tries = 1;
	for (;;) {
		if (write(x->fd, "I", 1) < 0) { 
			// TODO: do something with this
			return 0;
		} 

		printf("srv1_fill_image(): getting spec.\n");

		memset(specbuf, 0, 10);
		int done = read_limited(x->fd, specbuf, 10, 500000);

		if (done != 10) {
			int btsdead = srv1_flush_input(x);
			if (tries < 10) {
				tries++;
				// Try again!
				continue;
			} 
			// give up!
			printf("srv1_fill_image(): didn't get spec (got %d bytes: %s). flushed %d bytes.\n", done, specbuf, btsdead);
			return 0;
		} else {
			break; 
		} 
	} 

	if (strncmp("##IMJ", specbuf, 5) != 0) { 
		int btsdead = srv1_flush_input(x);
		printf("srv1_fill_image(): incorrect response from I, flushed %d\n", btsdead);
		return 0; 
	}
	int s0 = (unsigned char)specbuf[6];
	int s1 = (unsigned char)specbuf[7];
	int s2 = (unsigned char)specbuf[8];
	int s3 = (unsigned char)specbuf[9];

	x->frame_size = s0 + (s1 * 256) + (s2 * 256 * 256) + (s3 * 256 * 256 * 256);

	printf("srv1_fill_image(): specbuf: %c%c%c%c%c%c\n", specbuf[0], specbuf[1], specbuf[2], specbuf[3], specbuf[4], specbuf[5]);
	printf("srv1_fill_image(): frame_size = %d + (%d * 256) + (%d * 256 * 256) + (%d * 256 * 256 * 256) = %d\n",  s0, s1, s2, s3,  x->frame_size);
//	printf("srv1_fill_image(): spec size %d\n", x->frame_size);


	if (x->frame == NULL) { 
		x->frame = (char *)malloc(x->frame_size);
	} else { 
		x->frame = (char *)realloc(x->frame, x->frame_size);
	} 

	// 1.5 secs is long enough.
	read_limited(x->fd, x->frame, x->frame_size, 1500000);


	// CARLOS: explicitly, writing image to file (for testing only)
	savePhoto("test", x->frame, x->frame_size);

	return 1;
}

int srv1_fill_ir(srv1_comm_t *x) {
	if (write(x->fd, "B", 1) < 0) { 
		return 0; 
	} 

	char buf[80]; // Real length: 13 for header + 32 for chars.
	memset(buf, 0, 80);
	int done = read_limited(x->fd, buf, 46, 500000);

	if (done != 46) {
		// TODO: do something more important
		int btsdead = srv1_flush_input(x);
		printf("srv1_fill_ir(): discarded %d bytes.\n", btsdead);
		return 0;
	}

	if (strncmp(buf, "##BounceIR - ", 13) != 0) {
		int btsdead = srv1_flush_input(x);
		printf("srv1_fill_ir(): IR return had wrong header '%s', discarded %d bytes.\n", buf, btsdead);
		return 0;
	} 

	sscanf(buf + 13, "%x", &(x->bouncedir[0]));
	sscanf(buf + 21, "%x", &(x->bouncedir[1]));
	sscanf(buf + 29, "%x", &(x->bouncedir[2]));
	sscanf(buf + 37, "%x", &(x->bouncedir[3]));

	return 1;
} 

int srv1_read_sensors(srv1_comm_t *x) {
	
   // CARLOS: Needs to handle returns (1: success), (0: error)
	srv1_fill_image(x);
//	srv1_fill_ir(x);  // CARLOS: not tested yet

	return 1;
}

int srv1_reset_comms(srv1_comm_t *x) {
	return 1;
} 

double srv1_range_to_distance(int rangereading) {
	// Approximation based on a 3rd order polynomial fit of data: 
	//    -6.0333e-05 x^3 + 1.2986e-02 x^2 + -9.6280e-01 x + 4.3082e+01
	// TODO: Possibly replace this with a lookup table, or factorize
	//  to minimize numerical error.
	double a = -6.0333e-5 * rangereading * rangereading * rangereading; 
	double b = 1.2986e-2 * rangereading * rangereading; 
	double c = -9.6280e-1 * rangereading;
	double d = 4.3082e1;

	return a + b + c + d;
} 

// *************************************************
// Added method for testing Picture Delay issue:
int saveNamedData(const char *name, char *data, int size)
{
   int   fd;

   fd = open(name, O_TRUNC | O_CREAT | O_WRONLY, 00644);
   if (fd < 0)
      return 0;


   if (write(fd, data, size) != size)
   {
      close(fd);
      return 0;
   }
   close(fd);

   return 1;
}

void savePhoto(const std::string aPrefix, char *data, int size, uint32_t aWidth)
{
   static int nFrameNo = 0;

  std::ostringstream filename;
  filename.imbue(std::locale(""));
  filename.fill('0');

  filename << aPrefix << std::setw(aWidth) << nFrameNo++;
//  if (GetCompression())
    filename << ".jpg";
//  else
//    filename << ".ppm";

//  scoped_lock_t lock(mPc->mMutex);
    saveNamedData(filename.str().c_str(), data, size);
}


