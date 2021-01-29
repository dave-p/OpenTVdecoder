#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#ifdef OLDDVBAPI
#include <ost/dmx.h>
#define dmx_pes_filter_params dmxPesFilterParams
#define dmx_sct_filter_params dmxSctFilterParams
#else
#include <linux/dvb/dmx.h>
#endif

#include "../common.h"

#include "../core/log.h"

#include "dvb.h"

void dvb_read (dvb_t *settings, bool(*data_callback)(int, unsigned char*))
{
	if (no_dvb_poll)
	{
		int cycles, total_size, fd;
		struct dmx_sct_filter_params params;
	
		char first[settings->buffer_size];
		int first_length;
		bool first_ok;
	
		memset(&params, 0, sizeof(params));
		params.pid = settings->pid;
		params.filter.filter[0] = settings->filter;
		params.filter.mask[0] = settings->mask;
		params.timeout = 5000;
		params.flags = DMX_IMMEDIATE_START | DMX_CHECK_CRC;

		if ((fd = open(settings->demuxer, O_RDONLY|O_CLOEXEC|O_NONBLOCK)) < 0) {
			log_add ("Cannot open demuxer '%s'", settings->demuxer);
			return;
		}

		if (ioctl(fd, DMX_SET_FILTER, &params) == -1) {
			log_add ("ioctl DMX_SET_FILTER failed");
			close(fd);
			return;
		}

		log_add ("Reading pid 0x%x...", params.pid);

		first_length = 0;
		first_ok = false;
		total_size = 0;
		cycles = 0;
		while (cycles < MAX_OTV_LOOP_CYCLES)
		{
			//int k;
			//bool force_quit = false;
			unsigned char buf[settings->buffer_size];	// 4K buffer size
			int size = read (fd, buf, sizeof(buf));

			if (size == -1) {
				usleep (10 * 1000);
				continue;
			}

			if (size < settings->min_length) continue;

			if (first_length == 0)
			{
				first_length = size;
				memcpy (first, buf, size);
			}
			else if (first_length == size)
			{
				if (memcmp (buf, first, size) == 0) {
					first_ok = true;
				}
			}

			total_size += size;
			//data_callback (size, buf);
			if (!data_callback (size, buf)) {
				log_add ("Forced to quit");
				break;
			}

			if (first_ok) {
				log_add ("Done");
				break;
			}

			cycles++;
		}

		close(fd);
	}
	else
	{
		struct pollfd PFD[settings->pids_count];
		int cycles, i, total_size;
		struct dmx_sct_filter_params params;

		char first[settings->pids_count][settings->buffer_size];
		int first_length[settings->pids_count];
		bool first_ok[settings->pids_count];

		int dmx_next = 0;
		char dmx_adpt[256];
		memset(dmx_adpt, '\0', sizeof(dmx_adpt));

		if (carousel_dvb_poll)
		{
			strcpy(dmx_adpt, settings->demuxer);
			dmx_adpt[strlen(dmx_adpt)-1] = '\0';
		}
		else
			log_add ("Polling..%s", settings->demuxer);

		for (i = 0; i < settings->pids_count; i++)
		{
			if (carousel_dvb_poll)
			{
				char dmx_dev[256*2];
				memset(dmx_dev, '\0', 256*2);
				sprintf(dmx_dev, "%s%i", dmx_adpt, dmx_next++);
				if ((PFD[i].fd = open (dmx_dev, O_RDONLY|O_CLOEXEC|O_NONBLOCK)) < 0)
				{
					dmx_next = 0;
					memset(dmx_dev, '\0', 256*2);
					sprintf(dmx_dev, "%s%i", dmx_adpt, dmx_next++);
					PFD[i].fd = open (dmx_dev, O_RDONLY|O_CLOEXEC|O_NONBLOCK);
				}
				log_add ("Polling 0x%x..%s", settings->pids[i], dmx_dev);
			}
			else
				PFD[i].fd = open (settings->demuxer, O_RDONLY|O_CLOEXEC|O_NONBLOCK);

			PFD[i].events = POLLIN;
			PFD[i].revents = 0;

			memset (&params, 0, sizeof (params));
			params.pid = settings->pids[i];
			params.timeout = 5000;
			params.flags = DMX_CHECK_CRC|DMX_IMMEDIATE_START;
			params.filter.filter[0] = settings->filter;
			params.filter.mask[0] = settings->mask;

			if (ioctl (PFD[i].fd, DMX_SET_BUFFER_SIZE, settings->buffer_size * 4) < 0)
				log_add ("ioctl DMX_SET_BUFFER_SIZE failed");

			if (ioctl (PFD[i].fd, DMX_SET_FILTER, &params) < 0)
				log_add ("ioctl DMX_SET_FILTER failed");

			first_length[i] = 0;
			first_ok[i] = false;
		}

		total_size = 0;
		cycles = 0;
		while ((cycles < MAX_OTV_LOOP_CYCLES) && (poll (PFD, settings->pids_count, 5000) > 0))
		{
			int k;
			bool ended = true;
			bool force_quit = false;
			for (i = 0; i < settings->pids_count; i++)
			{
				unsigned char buf[settings->buffer_size];	// 4K buffer size
				int size = 0;
				if (PFD[i].revents & POLLIN)
					size = read (PFD[i].fd, buf, sizeof (buf));

				if (size == -1) continue;
				if (first_ok[i]) continue;
				if (size < settings->min_length) continue;

				if (first_length[i] == 0)
				{
					first_length[i] = size;
					memcpy (first[i], buf, size);
				}
				else if (first_length[i] == size)
				{
					if (memcmp (buf, first[i], size) == 0) first_ok[i] = true;
				}

				total_size += size;
				//data_callback (size, buf);
				force_quit = !data_callback (size, buf);
			}

			for (k = 0; k < settings->pids_count; k++)
				ended &= first_ok[k];

			if (ended || force_quit) break;

			cycles++;
		}

		if (cycles == MAX_OTV_LOOP_CYCLES) log_add ("Maximum loop exceded");

		for (i = 0; i < settings->pids_count; i++)	// close filters
		{
			if (ioctl (PFD[i].fd, DMX_STOP) < 0)
				log_add ("Error stopping filter");
			close (PFD[i].fd);
		}
	}
}
