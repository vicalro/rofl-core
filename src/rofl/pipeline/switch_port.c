#include "switch_port.h"

#include <string.h>
#include "platform/memory.h"


/**
* Inits a port structure including statistics 
*/
switch_port_t* switch_port_init(char* name, bool up, port_type_t type, port_state_t state){

	switch_port_t* port;

	//Allocate space
	port = cutil_malloc_shared(sizeof(switch_port_t));

	if(!port)
		return NULL;

	//Memset stats. Must be here!
	memset(&port->stats,0,sizeof(port->stats));

	//Init statistics mutex
	port->stats.mutex = platform_mutex_init(NULL);

	if(!port->stats.mutex){
		cutil_free_shared(port);
		return NULL;
	}

	//Fill values	
	port->type = type;
	port->up = up;
	port->state = state;

	//Copy name	
	strncpy(port->name, name, SWITCH_PORT_MAX_LEN_NAME);

	//Set to initial values the rest
	port->forward_packets = true;	
	port->drop_received = false; 
	port->curr = port->advertised = port->supported = port->peer = 0x0;	
	port->curr_speed = port->curr_max_speed = 0;
	port->of_port_num = 0;
	port->of_generate_packet_in = true;
	port->attached_sw = NULL;

	//Platform state
	port->platform_port_state = NULL;

	return port;
}

rofl_result_t switch_port_destroy(switch_port_t* port){

	//Free mutex
	platform_mutex_destroy(port->stats.mutex);
	
	//Free shared port
	cutil_free_shared(port);
	
	return ROFL_SUCCESS;
}

/**
* Increments atomically all the statistics of the port. Fill in with 0 the ones that should
* be left untouched.
*/
void switch_port_stats_inc(switch_port_t* port,
				uint64_t rx_packets,
				uint64_t tx_packets,
				uint64_t rx_bytes,
				uint64_t tx_bytes,
				uint64_t rx_dropped,
				uint64_t tx_dropped
				/*uint64_t rx_errors,
				uint64_t tx_errors,
				uint64_t rx_frame_err,
				uint64_t rx_over_err,
				uint64_t rx_crc_err,
				uint64_t collisions*/){
				
	//TODO: evaluate to use lock-less strategies (single threaded I/O subsystems)
	//or target platform specific atomic_incs
	platform_mutex_lock(port->stats.mutex);
	
	//Do all the stuff
	port->stats.rx_packets += rx_packets;
	port->stats.tx_packets += tx_packets;
	port->stats.rx_bytes += rx_bytes;
	port->stats.tx_bytes += tx_bytes;
	port->stats.rx_dropped += rx_dropped;
	port->stats.tx_dropped += tx_dropped;

/*
	port->stats.rx_errors += rx_errors;
	port->stats.tx_errors += tx_errors;
	port->stats.rx_frame_err += rx_frame_err;
	port->stats.rx_over_err += rx_over_err;
	port->stats.rx_crc_err += rx_crc_err;
	port->stats.collisions += collisions;
*/
	platform_mutex_unlock(port->stats.mutex);
}

/*
* Conveninent wrappers just to avoid messing up with the bitmaps
*/
void switch_port_add_capabilities(port_features_t* bitmap, port_features_t features){
	*bitmap |= features;
}
void switch_port_remove_capabilities(port_features_t* bitmap, port_features_t features){
	*bitmap &= (~features);
}
void switch_port_set_current_speed(switch_port_t* port, port_features_t speed){
	if(speed > PORT_FEATURE_1TB_FD)
		return;
	port->curr_speed = speed;
}
void switch_port_set_current_max_speed(switch_port_t* port, port_features_t speed){
	if(speed > PORT_FEATURE_1TB_FD)
		return;
	port->curr_max_speed = speed;
}