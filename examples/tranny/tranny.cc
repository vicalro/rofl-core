#include <iostream>
#include <iomanip>
#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <unistd.h>

#include "tranny.h"

#include <rofl/platform/unix/cunixenv.h>
#include <rofl/common/caddress.h>
#include <rofl/common/crofbase.h>
#include <rofl/common/cerror.h>
#include <rofl/common/openflow/openflow_rofl_exceptions.h>
#include <rofl/common/openflow/cofdpt.h>
#include <rofl/common/openflow/cofctl.h>
#include <rofl/common/openflow/openflow10.h>
#include <rofl/common/utils/c_logger.h>

// OF10 not supported until I can either get an OF10 version of rofl::send_flow_mod_message, or access crofbase::xids_used
#define PROXYOFPVERSION OFP10_VERSION		// OFP10_VERSION or OFP12_VERSION

std::string action_mask_to_string(const uint32_t action_types) {
	std::string out;
	static const uint32_t vals[] = { OFP10AT_OUTPUT, OFP10AT_SET_VLAN_VID, OFP10AT_SET_VLAN_PCP, OFP10AT_STRIP_VLAN, OFP10AT_SET_DL_SRC, OFP10AT_SET_DL_DST, OFP10AT_SET_NW_SRC, OFP10AT_SET_NW_DST, OFP10AT_SET_NW_TOS, OFP10AT_SET_TP_SRC, OFP10AT_SET_TP_DST, OFP10AT_ENQUEUE };
	static const std::string names[] = { "OFP10AT_OUTPUT", "OFP10AT_SET_VLAN_VID", "OFP10AT_SET_VLAN_PCP", "OFP10AT_STRIP_VLAN", "OFP10AT_SET_DL_SRC", "OFP10AT_SET_DL_DST", "OFP10AT_SET_NW_SRC", "OFP10AT_SET_NW_DST", "OFP10AT_SET_NW_TOS", "OFP10AT_SET_TP_SRC", "OFP10AT_SET_TP_DST", "OFP10AT_ENQUEUE" };
	static const size_t N = sizeof(vals)/sizeof(vals[0]);
	for(size_t i=0;i<N;++i) {
		if(action_types & (1<<vals[i])) out += names[i] + " ";
	}
	return out;
}


ctranslator::ctranslator():rofl::crofbase (1 <<  PROXYOFPVERSION),m_slave(0),m_master(0) {
//ctranslator::ctranslator():rofl::crofbase ((1 << OFP10_VERSION)|(1 <<  OFP12_VERSION)),m_slave(0),m_master(0),m_of_version(OFP12_VERSION) {
	// read config file
	//..
}

ctranslator::~ctranslator() {
	// rpc_close_all();
	std::cout << std::endl << "ctranslator::~ctranslator() called." << std::endl;	// TODO: proper logging
}

void ctranslator::handle_error (rofl::cofdpt *dpt, rofl::cofmsg *msg) {
	std::cout << std::endl << "handle_error from " << dpt->c_str() << " : " << msg->c_str() << std::endl;
}

void ctranslator::handle_ctrl_open (rofl::cofctl *ctl) {
	// should be called automatically after call to rpc_connect_to_dpt in connect_to_slave
	std::cout << std::endl << "ctranslator::handle_ctrl_open called with " << (ctl?ctl->c_str():"NULL") << std::endl;
	m_master = ctl;	// TODO - what to do with previous m_master?
}

void ctranslator::handle_ctrl_close (rofl::cofctl *ctl) {
	std::cout << "ctranslator::handle_ctrl_close called with " << (ctl?ctl->c_str():"NULL") << std::endl;
	// controller disconnected - now disconnect from switch.
	rpc_disconnect_from_dpt(m_slave);
	// hopefully dpt will reconnect to us, causing us in turn to reconnect to ctl.
	
	// this socket disconnecting could just be a temporary thing - mark it is dead, but expect a possible auto reconnect
	if(ctl!=m_master) std::cout << "ctranslator::handle_ctrl_close: was expecting " << (m_master?m_master->c_str():"NULL") << " but got " << (ctl?ctl->c_str():"NULL") << std::endl;
	m_master=0;	// TODO - m_naster ownership?
}

// This function feels alone, ignored, never called by anybody.
// And I'm mad as hell and I'm not going to take it any more!
void ctranslator::handle_dpath_open (rofl::cofdpt *dpt) {
	// should be called automatically after call to rpc_connect_to_dpt in connect_to_slave
	std::cout << std::endl << "ctranslator::handle_dpath_open called with " << (dpt?dpt->c_str():"NULL") << std::endl;
	m_slave = dpt;	// TODO - what to do with previous m_slave?
	m_slave_dpid=dpt->get_dpid();	// TODO - check also get_config, get_capabilities etc
	m_dpid = m_slave_dpid + 1;
//	rpc_connect_to_ctl(m_of_version,3,rofl::caddress(AF_INET, "127.0.0.1", 6633));
	rpc_connect_to_ctl(PROXYOFPVERSION,3,rofl::caddress(AF_INET, "127.0.0.1", 6633));
}

void ctranslator::handle_dpath_close (rofl::cofdpt *dpt) {
	std::cout << std::endl << "handle_dpath_close called with " << (dpt?dpt->c_str():"NULL") << std::endl;
//	assert(dpt==m_slave);
	if(dpt!=m_slave) std::cout << "ctranslator::handle_dpath_close: Was expecting " << (m_slave?m_slave->c_str():"NULL") << " but got " << (dpt?dpt->c_str():"NULL") << std::endl;
	// this socket disconnecting could just be a temporary thing - mark it is dead, but expect a possible auto reconnect
	m_slave=0;	// TODO - m_slave ownership?
}

void ctranslator::handle_flow_mod(rofl::cofctl *ctl, rofl::cofmsg_flow_mod *msg) {
	static const char * func = __FUNCTION__;
	std::cout << std::endl << func << " from " << ctl->c_str() << " : " << msg->c_str() << "." << std::endl;
	rofl::cflowentry entry(OFP10_VERSION);
	try {
	//BOOST_PP_SEQ_FOR_EACH( CLONECMD, ((*msg))(entry), (command)(idle_timeout)(hard_timeout)(cookie)(priority)(buffer_id)(out_port)(flags) )
		entry.set_command(msg->get_command());
		entry.set_idle_timeout(msg->get_idle_timeout());
		entry.set_hard_timeout(msg->get_hard_timeout());
		entry.set_cookie(msg->get_cookie());
		entry.set_priority(msg->get_priority());
		entry.set_buffer_id(msg->get_buffer_id());
		entry.set_out_port(msg->get_out_port());
		entry.set_flags(msg->get_flags());
		send_flow_mod_message( m_slave, entry );
		} catch (rofl::cerror &e) {
		std::cout << func << ": caught " << e << ". at " << __FILE__ << ":" << __LINE__ << std::endl;
	}
	delete(msg);
}

void ctranslator::handle_features_request(rofl::cofctl *ctl, rofl::cofmsg_features_request *pack) {
	static const char * func = __FUNCTION__;
	std::cout << std::endl << func << " from " << ctl->c_str() << " : " << pack->c_str() << std::endl;
	uint32_t masterxid = pack->get_xid();
	if(m_mxid_sxid.find(masterxid)!=m_mxid_sxid.end()) std::cout << "ERROR: ctranslator::handle_features_request : xid from incoming cofmsg_features_request already exists in m_mxid_sxid" << std::endl;
	// send features request
	if(m_slave) {
		uint32_t myxid = send_features_request(m_slave);
		std::cout << "handle_features_request called send_features_request(" << m_slave->c_str() << ")." << std::endl;
		if(m_sxid_mxid.find(myxid)!=m_sxid_mxid.end()) std::cout << "ERROR: ctranslator::handle_features_request : xid from send_features_request already exists in m_sxid_mxid" << std::endl;
		m_mxid_sxid[masterxid]=myxid;
		m_sxid_mxid[myxid]=masterxid;
	} else std::cout << "handle_features_request from " << ctl->c_str() << " dropped because no connection to datapath present." << std::endl;
	delete(pack);
}

void ctranslator::handle_features_reply(rofl::cofdpt * dpt, rofl::cofmsg_features_reply * msg ) {
	static const char * func = __FUNCTION__;
	std::cout << std::endl << func << " from " << dpt->c_str() << " : " << msg->c_str() << std::endl;
	uint32_t actions = msg->get_actions_bitmap();
	uint32_t myxid = msg->get_xid();
	xid_map_t::iterator map_it = m_sxid_mxid.find(myxid);
	if(map_it==m_sxid_mxid.end()) { std::cout << "ERROR: ctranslator::handle_features_reply : xid from reply doesn't exist in m_sxid_mxid" << std::endl; return; }
	uint32_t orig_xid = map_it->second;
	
	// Now, remove VLAN setters and queue features from actions
	std::cout << func << ": Features reply (" << std::hex << actions << std::dec << "): " <<  action_mask_to_string(actions) << std::endl;
	// make changes to actions
	actions &= ~((uint32_t)(1<<OFP10AT_ENQUEUE));
	actions &= ~((uint32_t)(1<<OFP10AT_SET_VLAN_VID));
	actions &= ~((uint32_t)(1<<OFP10AT_SET_VLAN_PCP));
	std::cout << func << ": New features are (" << std::hex << actions << std::dec << "): " <<  action_mask_to_string(actions) << std::endl;
	
	send_features_reply(m_master, orig_xid, m_dpid, msg->get_n_buffers(), msg->get_n_tables(), msg->get_capabilities(), 0, msg->get_actions_bitmap(), msg->get_ports() );	// TODO get_action_bitmap is OF1.0 only
	std::cout << "handle_features_reply: sent features reply to " << m_master->c_str() << "." << std::endl;
	m_sxid_mxid.erase(map_it);
	m_mxid_sxid.erase(orig_xid);
	delete(msg);
}
void ctranslator::handle_get_config_request(rofl::cofctl *ctl, rofl::cofmsg_get_config_request *msg) {
	static const char * func = __FUNCTION__;
	std::cout << std::endl << func << " from " << ctl->c_str() << " : " << msg->c_str() << std::endl;
	uint32_t masterxid = msg->get_xid();
	if(m_mxid_sxid.find(masterxid)!=m_mxid_sxid.end()) std::cout << "ERROR: " << func << " : xid from incoming cofmsg_get_config_request already exists in m_mxid_sxid" << std::endl;
	if(m_slave) {
		uint32_t myxid = send_get_config_request(m_slave);
		std::cout << func << " called send_get_config_request(" << m_slave->c_str() << ")." << std::endl;
		if(m_sxid_mxid.find(myxid)!=m_sxid_mxid.end()) std::cout << "ERROR: " << func << " : xid from send_get_config_request already exists in m_sxid_mxid" << std::endl;
		m_mxid_sxid[masterxid]=myxid;
		m_sxid_mxid[myxid]=masterxid;
	} else std::cout << func << " from " << ctl->c_str() << " dropped because no connection to datapath present." << std::endl;
	delete(msg);
}

void ctranslator::handle_get_config_reply(rofl::cofdpt * dpt, rofl::cofmsg_get_config_reply * msg ) {
	static const char * func = __FUNCTION__;
	std::cout << std::endl << func << " from " << dpt->c_str() << " : " << msg->c_str() << std::endl;
	uint32_t myxid = msg->get_xid();
	xid_map_t::iterator map_it = m_sxid_mxid.find(myxid);
	if(map_it==m_sxid_mxid.end()) { std::cout << "ERROR: " << func << " : xid from slave's reply doesn't exist in m_sxid_mxid" << std::endl; return; }
	uint32_t orig_xid = map_it->second;
	send_get_config_reply(m_master, orig_xid, msg->get_flags(), msg->get_miss_send_len() );
	std::cout << func << " : sent reply to " << m_master->c_str() << "." << std::endl;
	m_sxid_mxid.erase(map_it);
	m_mxid_sxid.erase(orig_xid);
	delete(msg);
}

void ctranslator::handle_desc_stats_request(rofl::cofctl *ctl, rofl::cofmsg_desc_stats_request *msg) {
	static const char * func = __FUNCTION__;
	std::cout << std::endl << func << " from " << ctl->c_str() << " : " << msg->c_str() << std::endl;
	uint32_t masterxid = msg->get_xid();
	if(m_mxid_sxid.find(masterxid)!=m_mxid_sxid.end()) std::cout << "ERROR: " << func << " : xid from incoming cofmsg_desc_stats_request already exists in m_mxid_sxid" << std::endl;
	if(m_slave) {
		uint32_t myxid = send_desc_stats_request(m_slave, msg->get_stats_flags());
		std::cout << func << " called send_desc_stats_request(" << m_slave->c_str() << " ..)." << std::endl;
		if(m_sxid_mxid.find(myxid)!=m_sxid_mxid.end()) std::cout << "ERROR: " << func << " : xid from send_desc_stats_request already exists in m_sxid_mxid" << std::endl;
		m_mxid_sxid[masterxid]=myxid;
		m_sxid_mxid[myxid]=masterxid;
	} else std::cout << func << " from " << ctl->c_str() << " dropped because no connection to datapath present." << std::endl;
	delete(msg);
}

void ctranslator::handle_desc_stats_reply(rofl::cofdpt * dpt, rofl::cofmsg_desc_stats_reply * msg) {
	static const char * func = __FUNCTION__;
	std::cout << std::endl << func << " from " << dpt->c_str() << " : " << msg->c_str() << std::endl;
	uint32_t myxid = msg->get_xid();
	xid_map_t::iterator map_it = m_sxid_mxid.find(myxid);
	if(map_it==m_sxid_mxid.end()) { std::cout << "ERROR: " << func << " : xid from slave's reply doesn't exist in m_sxid_mxid" << std::endl; return; }
	uint32_t orig_xid = map_it->second;
	// TODO this bit below is wrong, but I cannot find a proper description of cofmsg_desc_stats_reply to get the information from it
	rofl::cofdesc_stats_reply reply(dpt->get_version(),"tranny_mfr_desc","tranny_hw_desc","tranny_sw_desc","tranny_serial_num","tranny_dp_desc");
	send_desc_stats_reply(m_master, orig_xid, reply, false );
	std::cout << func << " : sent desc stats reply to " << m_master->c_str() << "." << std::endl;
	m_sxid_mxid.erase(map_it);
	m_mxid_sxid.erase(orig_xid);
	delete(msg);
}

// "Called once an EXPERIMENTER.message was received from a controller entity. ".. or is it? The docs are wrong on this one.
void ctranslator::handle_set_config(rofl::cofctl *ctl, rofl::cofmsg_set_config *msg) {
	static const char * func = __FUNCTION__;
	std::cout << std::endl << func << " from " << ctl->c_str() << " : " << msg->c_str() << std::endl;
	send_set_config_message(m_slave, msg->get_flags(), msg->get_miss_send_len());
	std::cout << func << " : sent set_config_message to " << m_slave->c_str() << "." << std::endl;
	delete (msg);	
}

void ctranslator::handle_table_stats_request(rofl::cofctl *ctl, rofl::cofmsg_table_stats_request *msg) {
	static const char * func = __FUNCTION__;
	std::cout << std::endl << func << " from " << ctl->c_str() << " : " << msg->c_str() << std::endl;
	uint32_t masterxid = msg->get_xid();
	if(m_mxid_sxid.find(masterxid)!=m_mxid_sxid.end()) std::cout << "ERROR: " << func << " : xid from incoming cofmsg_table_stats_request already exists in m_mxid_sxid" << std::endl;
	if(m_slave) {
		uint32_t myxid = send_table_stats_request(m_slave, msg->get_stats_flags());
		std::cout << func << " called send_table_stats_request(" << m_slave->c_str() << " ..)." << std::endl;
		if(m_sxid_mxid.find(myxid)!=m_sxid_mxid.end()) std::cout << "ERROR: " << func << " : xid from cofmsg_table_stats_request already exists in m_sxid_mxid" << std::endl;
		m_mxid_sxid[masterxid]=myxid;
		m_sxid_mxid[myxid]=masterxid;
	} else std::cout << func << " from " << ctl->c_str() << " dropped because no connection to datapath present." << std::endl;
	delete(msg);
}
void ctranslator::handle_table_stats_reply(rofl::cofdpt *dpt, rofl::cofmsg_table_stats_reply *msg) {
	static const char * func = __FUNCTION__;
	std::cout << std::endl << func << " from " << dpt->c_str() << " : " << msg->c_str() << std::endl;
	uint32_t myxid = msg->get_xid();
	xid_map_t::iterator map_it = m_sxid_mxid.find(myxid);
	if(map_it==m_sxid_mxid.end()) { std::cout << "ERROR: " << func << " : xid from slave's reply doesn't exist in m_sxid_mxid" << std::endl; return; }
	uint32_t orig_xid = map_it->second;
	send_table_stats_reply(m_master, orig_xid, msg->get_table_stats(), false ); // TODO how to deal with "more" flag (last arg)
	std::cout << func << " : sent reply to " << m_master->c_str() << "." << std::endl;
	m_sxid_mxid.erase(map_it);
	m_mxid_sxid.erase(orig_xid);
	delete(msg);
}

void ctranslator::handle_port_stats_request(rofl::cofctl *ctl, rofl::cofmsg_port_stats_request *pack) {
	static const char * func = __FUNCTION__;
	std::cout << std::endl << func << " from " << ctl->c_str() << " : " << pack->c_str() << std::endl;
}
void ctranslator::handle_flow_stats_request(rofl::cofctl *ctl, rofl::cofmsg_flow_stats_request *pack) {
// see ./examples/etherswitch/etherswitch.cc:95
	static const char * func = __FUNCTION__;
	std::cout << std::endl << func << " from " << ctl->c_str() << " : " << pack->c_str() << std::endl;
}
void ctranslator::handle_aggregate_stats_request(rofl::cofctl *ctl, rofl::cofmsg_aggr_stats_request *pack) {
	static const char * func = __FUNCTION__;
	std::cout << std::endl << func << " from " << ctl->c_str() << " : " << pack->c_str() << std::endl;
	uint32_t masterxid = pack->get_xid();
	if(m_mxid_sxid.find(masterxid)!=m_mxid_sxid.end()) std::cout << "ERROR: " << func << " : xid from incoming handle_aggregate_stats_request already exists in m_mxid_sxid" << std::endl;
	if(m_slave) {
std::cout << "TP" << __LINE__ << std::endl;
		uint16_t stats_flags = pack->get_stats_flags();
std::cout << "TP" << __LINE__ << std::endl;
		rofl::cofaggr_stats_request aggr_req( pack->get_aggr_stats() );
std::cout << "TP" << __LINE__ << std::endl;
		uint32_t myxid = send_aggr_stats_request(m_slave, stats_flags, aggr_req);
std::cout << "TP" << __LINE__ << std::endl;
		std::cout << func << " called send_aggr_stats_request(" << m_slave->c_str() << " ..)." << std::endl;
		if(m_sxid_mxid.find(myxid)!=m_sxid_mxid.end()) std::cout << "ERROR: " << func << " : xid from handle_aggregate_stats_request already exists in m_sxid_mxid" << std::endl;
		m_mxid_sxid[masterxid]=myxid;
		m_sxid_mxid[myxid]=masterxid;
	} else std::cout << func << " from " << ctl->c_str() << " dropped because no connection to datapath present." << std::endl;
	delete(pack);
}
void ctranslator::handle_aggregate_stats_reply(rofl::cofdpt *dpt, rofl::cofmsg_aggr_stats_reply *msg) {
	static const char * func = __FUNCTION__;
	std::cout << std::endl << func << " from " << dpt->c_str() << " : " << msg->c_str() << std::endl;
	uint32_t myxid = msg->get_xid();
	xid_map_t::iterator map_it = m_sxid_mxid.find(myxid);
	if(map_it==m_sxid_mxid.end()) { std::cout << "ERROR: " << func << " : xid from slave's reply doesn't exist in m_sxid_mxid" << std::endl; return; }
	uint32_t orig_xid = map_it->second;
	send_aggr_stats_reply(m_master, orig_xid, msg->get_aggr_stats(), false );	// TODO how to deal with "more" flag (last arg)
	std::cout << func << " : sent reply to " << m_master->c_str() << "." << std::endl;
	m_sxid_mxid.erase(map_it);
	m_mxid_sxid.erase(orig_xid);
	delete(msg);
}


void ctranslator::handle_queue_stats_request(rofl::cofctl *ctl, rofl::cofmsg_queue_stats_request *pack) {
	static const char * func = __FUNCTION__;
	std::cout << std::endl << func << " from " << ctl->c_str() << " : " << pack->c_str() << std::endl;
}
void ctranslator::handle_experimenter_stats_request(rofl::cofctl *ctl, rofl::cofmsg_stats_request *pack) {
	static const char * func = __FUNCTION__;
	std::cout << std::endl << func << " from " << ctl->c_str() << " : " << pack->c_str() << std::endl;
}

void dumpBytes (std::ostream & os, uint8_t * bytes, size_t n_bytes) {
/*	std::ios_base::fmtflags fmt = os.flags();
	os << std::hex << std::setfill('0') << std::setw(2);
	std::copy(&bytes[0], &bytes[n_bytes], std::ostream_iterator<uint8_t>(os, " "));
	os.setf( fmt );*/
	if (0==n_bytes) return;
	for(size_t i = 0; i < (n_bytes-1); ++i) printf("%02x ", bytes[i]);
	printf("%02x", bytes[n_bytes-1]);
}

void ctranslator::handle_packet_in(rofl::cofdpt *dpt, rofl::cofmsg_packet_in * msg) {
	static const char * func = __FUNCTION__;
	std::cout << std::endl << func << " from " << dpt->c_str() << " : " << msg->c_str() << "\n" << *msg << std::endl;
	// forward packet to master
std::cout << "TP" << __LINE__ << std::endl;
	rofl::cofmatch match(msg->get_match_const());
std::cout << "TP" << __LINE__ << "match found to be " << match.c_str() << std::endl;	
std::cout << "TP" << __LINE__ << std::endl;
	rofl::cpacket packet(msg->get_packet_const());
std::cout << "TP" << __LINE__ << std::endl;
std::cout << "packet.framelen = " << (unsigned)packet.framelen() << "packet.soframe = " << packet.soframe() << std::endl;
std::cout << "TP" << __LINE__ << std::endl;
	packet.get_match().set_in_port(msg->get_in_port());
std::cout << "TP" << __LINE__ << std::endl;
std::cout << "packet bytes: ";
dumpBytes(std::cout,msg->get_packet_const().soframe(), msg->get_packet_const().framelen());
std::cout << std::endl;
std::cout << "frame bytes: ";
dumpBytes(std::cout,msg->get_packet().frame()->soframe(), msg->get_packet().frame()->framelen());
std::cout << "TP" << __LINE__ << std::endl;
std::cout << "source MAC: " << msg->get_packet().ether()->get_dl_src() << std::endl;
std::cout << "dest MAC: " << msg->get_packet().ether()->get_dl_dst() << std::endl;
std::cout << "OFP10_PACKET_IN_STATIC_HDR_LEN is " << OFP10_PACKET_IN_STATIC_HDR_LEN << std::endl;
// *** JSP Continue working from here - The data returned by msg->get_packet() is missing the first four bytes, and because of this the printed MAC addresses are wrong.
// ** TODO: check who creates msg and figure out why it "starts late"
std::cout << "TP" << __LINE__ << std::endl;
	send_packet_in_message(m_master, msg->get_buffer_id(), msg->get_total_len(), msg->get_reason(), 0, 0, msg->get_in_port(), match, packet.ether()->sopdu(), packet.framelen() );	// TODO - the length fields are guesses.
//	send_packet_in_message(m_master, msg->get_buffer_id(), msg->get_total_len(), msg->get_reason(), 0, 0, msg->get_in_port(), match, packet.soframe(), packet.framelen() );	// TODO - the length fields are guesses.
//	send_packet_in_message(m_master, msg->get_buffer_id(), msg->get_total_len(), msg->get_reason(), 0, 0, msg->get_in_port(), match, packet.frame()->sopdu(), packet.frame()->pdulen() );	// TODO - the length fields are guesses.
	std::cout << func << " : packet_in forwarded to " << m_master->c_str() << "." << std::endl;
#if 0
std::cout << "TP" << __LINE__ << std::endl;
	std::cout << "dpt version is " << (unsigned)dpt->get_version() << std::endl;
std::cout << "TP" << __LINE__ << std::endl;
//	send_packet_in_message(m_master, msg->get_buffer_id(), msg->get_total_len(), msg->get_reason(), msg->get_table_id(), msg->get_cookie(), msg->get_in_port(), match, packet.soframe(), packet.framelen() );	// TODO - the length fields are guesses.
		uint32_t  	buffer_id_ = msg->get_buffer_id();
std::cout << "TP" << __LINE__ << std::endl;
		uint16_t  	total_len_ = msg->get_total_len();
std::cout << "TP" << __LINE__ << std::endl;
		uint8_t  	reason_ = msg->get_reason();
//		uint8_t  	table_id,
std::cout << "TP" << __LINE__ << std::endl;
//		uint64_t  	cookie_ = msg->get_cookie();
uint64_t  	cookie_ = 0;
std::cout << "TP" << __LINE__ << std::endl;
		uint16_t  	in_port_ = msg->get_in_port();
std::cout << "TP" << __LINE__ << std::endl;
	send_packet_in_message(m_master, buffer_id_, total_len_, reason_, 0, cookie_, in_port_, match, packet.soframe(), packet.framelen() );	// TODO - the length fields are guesses.
std::cout << "TP" << __LINE__ << std::endl;
#endif

	delete(msg);
}
void ctranslator::handle_packet_out(rofl::cofctl *ctl, rofl::cofmsg_packet_out *pack) {
	static const char * func = __FUNCTION__;
	std::cout << std::endl << func << " from " << ctl->c_str() << " : " << pack->c_str() << std::endl;
	rofl::cofaclist actions(pack->get_actions());
	std::cout << "TP" << __LINE__ << std::endl;
	rofl::cpacket packet(pack->get_packet());
	std::cout << "TP" << __LINE__ << std::endl;
	send_packet_out_message(m_slave, pack->get_buffer_id(), pack->get_in_port(), actions, packet.soframe(), packet.framelen() );	// TODO - the length fields are guesses.
	std::cout << "TP" << __LINE__ << std::endl;
	delete(pack);
}
void ctranslator::handle_barrier_request(rofl::cofctl *ctl, rofl::cofmsg_barrier_request *pack) {
	static const char * func = __FUNCTION__;
	std::cout << std::endl << func << " from " << ctl->c_str() << " : " << pack->c_str() << std::endl;
	uint32_t masterxid = pack->get_xid();
	if(m_mxid_sxid.find(masterxid)!=m_mxid_sxid.end()) std::cout << "ERROR: " << func << " : xid from incoming handle_barrier_request already exists in m_mxid_sxid" << std::endl;
	if(m_slave) {
		uint32_t myxid = send_barrier_request(m_slave);
		if(m_sxid_mxid.find(myxid)!=m_sxid_mxid.end()) std::cout << "ERROR: " << func << " : xid from handle_barrier_request already exists in m_sxid_mxid" << std::endl;
		m_mxid_sxid[masterxid]=myxid;
		m_sxid_mxid[myxid]=masterxid;
	} else std::cout << func << " from " << ctl->c_str() << " dropped because no connection to datapath present." << std::endl;
	delete(pack);
}

void ctranslator::handle_barrier_reply ( rofl::cofdpt * dpt, rofl::cofmsg_barrier_reply * msg) {
	static const char * func = __FUNCTION__;
	std::cout << std::endl << func << " from " << dpt->c_str() << " : " << msg->c_str() << std::endl;
	uint32_t myxid = msg->get_xid();
	xid_map_t::iterator map_it = m_sxid_mxid.find(myxid);
	if(map_it==m_sxid_mxid.end()) { std::cout << "ERROR: " << func << " : xid from slave's reply doesn't exist in m_sxid_mxid" << std::endl; return; }
	uint32_t orig_xid = map_it->second;
	send_barrier_reply(m_master, orig_xid );
	std::cout << func << " : sent reply to " << m_master->c_str() << "." << std::endl;
	m_sxid_mxid.erase(map_it);
	m_mxid_sxid.erase(orig_xid);
	delete(msg);
}

void ctranslator::handle_table_mod(rofl::cofctl *ctl, rofl::cofmsg_table_mod *pack) {
	static const char * func = __FUNCTION__;
	std::cout << std::endl << func << " from " << ctl->c_str() << " : " << pack->c_str() << std::endl;
	delete(pack);
}
void ctranslator::handle_port_mod(rofl::cofctl *ctl, rofl::cofmsg_port_mod *pack) {
	static const char * func = __FUNCTION__;
	std::cout << std::endl << func << " from " << ctl->c_str() << " : " << pack->c_str() << std::endl;
	delete(pack);
}
void ctranslator::handle_queue_get_config_request(rofl::cofctl *ctl, rofl::cofmsg_queue_get_config_request *pack) {
	static const char * func = __FUNCTION__;
	std::cout << std::endl << func << " from " << ctl->c_str() << " : " << pack->c_str() << std::endl;
	delete(pack);
}
void ctranslator::handle_experimenter_message(rofl::cofctl *ctl, rofl::cofmsg_features_request *pack) {
	static const char * func = __FUNCTION__;
	std::cout << std::endl << func << " from " << ctl->c_str() << " : " << pack->c_str() << std::endl;
	delete(pack);
}





#if 0
// this function will take a flow_mod message and repackage and send it to the DPE
void ctranslator::handle_flow_mod(rofl::cofctl *ctl, rofl::cofmsg_flow_mod *msg) {
	static const char * func = __FUNCTION__;
	std::cout << std::endl << func << " from " << ctl->c_str() << " : " << msg->c_str() << "." << std::endl;
	
	std::cout << func << ": Attempt to send flow_mod with crofbase::send_flow_mod_message( cofdpt *dpt, cofmatch& ofmatch,uint64_t cookie,..cofinlist& inlist)." << std::endl;
	try {
		rofl::cofinst_apply_actions cofac(OFP10_VERSION);
		std::cout << "TP" << __LINE__ << std::endl;
		cofac.actions = msg->get_actions();
		std::cout << "TP" << __LINE__ << std::endl;
		rofl::cofinlist instructions_(OFP10_VERSION);
		std::cout << "TP" << __LINE__ << std::endl;
		instructions_.next() = cofac;
		std::cout << "TP" << __LINE__ << std::endl;
		rofl::cofmatch match_(msg->get_match());
		std::cout << "TP" << __LINE__ << std::endl;
		uint64_t cookie_;
		uint64_t cookie_mask_ = 0;
		uint8_t table_id_;
		uint8_t command_;
		uint16_t idle_timeout_;
		uint16_t hard_timeout_;
		uint16_t priority_;
		uint32_t buffer_id_;
		uint32_t out_port_;
		uint32_t out_group_ = 0;
		uint16_t flags_;

		std::cout << "TP" << __LINE__ << std::endl;
		cookie_ = msg->get_cookie();
		std::cout << "TP" << __LINE__ << std::endl;
//				cookie_mask_ = msg->get_cookie_mask();	// only OFP12 and OFP13
		std::cout << "TP" << __LINE__ << std::endl;
		table_id_ = msg->get_table_id();
		std::cout << "TP" << __LINE__ << std::endl;
		command_ = msg->get_command();
		std::cout << "TP" << __LINE__ << std::endl;
		idle_timeout_ = msg->get_idle_timeout();
		std::cout << "TP" << __LINE__ << std::endl;
		hard_timeout_ = msg->get_hard_timeout();
		std::cout << "TP" << __LINE__ << std::endl;
		priority_ = msg->get_priority();
		std::cout << "TP" << __LINE__ << std::endl;
		buffer_id_ = msg->get_buffer_id();
		std::cout << "TP" << __LINE__ << std::endl;
		out_port_ = msg->get_out_port();
		std::cout << "TP" << __LINE__ << std::endl;
//				out_group_ = msg->get_out_group();		// only OFP12 and OFP13
		std::cout << "TP" << __LINE__ << std::endl;
		flags_ = msg->get_flags();
		std::cout << "TP" << __LINE__ << std::endl;
		send_flow_mod_message( m_slave, match_, cookie_, cookie_mask_, table_id_, command_, idle_timeout_, hard_timeout_, priority_, buffer_id_, out_port_, out_group_, flags_, instructions_ );
		std::cout << "TP" << __LINE__ << std::endl;
		std::cout << "send_flow_mod_message (cofdpt *dpt..cofinlist& inlist) completed." << std::endl;

	} catch (rofl::eBadVersion &e) {
		std::cout << func << ": eBadVersion caught: " << e << ". at " << __FILE__ << ":" << __LINE__ << std::endl;
	} catch (rofl::cerror &e) {
		std::cout << func << ": caught " << e << ". at " << __FILE__ << ":" << __LINE__ << std::endl;
	}

	
	std::cout << func << ": Attempt to send flow_mod with send_flow_mod_message( cofdpt * dpt, cflowentry & flowentry )." << std::endl;
	try {
		std::cout << "TP" << __LINE__ << std::endl;
		rofl::cflowentry entry(OFP10_VERSION);		// work it, yeah baby!
		std::cout << "TP" << __LINE__ << std::endl;
		entry.set_command(msg->get_command());
		std::cout << "TP" << __LINE__ << std::endl;
//		entry.set_table_id(msg->get_table_id());
		std::cout << "TP" << __LINE__ << std::endl;
		entry.set_idle_timeout(msg->get_idle_timeout());
		std::cout << "TP" << __LINE__ << std::endl;
		entry.set_hard_timeout(msg->get_hard_timeout());
		std::cout << "TP" << __LINE__ << std::endl;
		entry.set_cookie(msg->get_cookie());
		std::cout << "TP" << __LINE__ << std::endl;
//		entry.set_cookie_mask(msg->get_cookie_mask());
		std::cout << "TP" << __LINE__ << std::endl;
		entry.set_priority(msg->get_priority());
		std::cout << "TP" << __LINE__ << std::endl;
		entry.set_buffer_id(msg->get_buffer_id());
		std::cout << "TP" << __LINE__ << std::endl;
		entry.set_out_port(msg->get_out_port());
		std::cout << "TP" << __LINE__ << std::endl;
//		entry.set_out_group(msg->get_out_group());
		std::cout << "TP" << __LINE__ << std::endl;
		entry.set_flags(msg->get_flags());
		std::cout << "TP" << __LINE__ << std::endl;
		entry.pack();
		std::cout << "TP" << __LINE__ << std::endl;
		send_flow_mod_message( m_slave, entry );
		std::cout << "TP" << __LINE__ << std::endl;
		std::cout << "send_flow_mod_message(cofdpt * dpt, cflowentry & flowentry)." << std::endl;
	} catch (rofl::eBadVersion &e) {
		std::cout << func << ": eBadVersion caught: " << e << ". at " << __FILE__ << ":" << __LINE__ << std::endl;
	} catch (rofl::cerror &e) {
		std::cout << func << ": caught " << e << ". at " << __FILE__ << ":" << __LINE__ << std::endl;
	}


//	std::cout << func << ": Attempt to send flow_mod with ???." << std::endl;


	
}
#endif

#if 0
void ctranslator::handle_flow_mod(rofl::cofctl *ctl, rofl::cofmsg_flow_mod *msg) {
	static const char * func = __FUNCTION__;
	std::cout << std::endl << func << " from " << ctl->c_str() << " : " << msg->c_str() << "." << std::endl;
///	rofl::cflowentry entry(m_of_version);	// this constructor may be broken - just throws rofl::eBadVersion whatever the version is.
///	std::cout << "TP" << __LINE__ << std::endl;
/*	entry.set_command(msg->get_command());
	entry.set_table_id(msg->get_table_id());
	entry.set_idle_timeout(msg->get_idle_timeout());
	entry.set_hard_timeout(msg->get_hard_timeout());
	entry.set_cookie(msg->get_cookie());
	entry.set_cookie_mask(msg->get_cookie_mask());
	entry.set_priority(msg->get_priority());
	entry.set_buffer_id(msg->get_buffer_id());
	entry.set_out_port(msg->get_out_port());
	entry.set_out_group(msg->get_out_group());
	entry.set_flags(msg->get_flags()); */
/// BOOST_PP_SEQ_FOR_EACH( CLONECMD, ((*msg))(entry), (command)(table_id)(idle_timeout)(hard_timeout)(cookie)(cookie_mask)(priority)(buffer_id)(out_port)(out_group)(flags) )
///	send_flow_mod_message( m_slave, entry );
	try {
#if 0
		if(m_of_version==OFP10_VERSION) {
			rofl::cflowentry entry(OFP10_VERSION);
			BOOST_PP_SEQ_FOR_EACH( CLONECMD, ((*msg))(entry), (command)(table_id)(idle_timeout)(hard_timeout)(cookie)(cookie_mask)(priority)(buffer_id)(out_port)(out_group)(flags) )
/*			cofmsg_flow_mod	*pack = new cofmsg_flow_mod( OFP10_VERSION, ta_new_async_xid(), msg->get_cookie(), msg->get_cookie_mask(), \
				msg->get_table_id(), msg->get_command(), msg->get_idle_timeout(), msg->get_hard_timeout(), msg->get_priority(), \
				msg->get_buffer_id(), msg->get_out_port(), msg->get_out_group(), msg->get_flags(),
                                        inlist,
                                        ofmatch);
			pack->pack();
			m_slave->send_message(pack);*/
			entry.pack();
			send_flow_mod_message( m_slave, entry );
		} else {
			// TODO test this OF12 code
			assert(false);
			std::cout << func << ": About to create rofl::cofmatch." << std::endl;
			rofl::cofmatch match_(msg->get_match());
			std::cout << func << ": About to create rofl::cofinlist." << std::endl;
		/// get_instructions is not valid for < OF12
			rofl::cofinlist instructions_(msg->get_instructions());		// JSP errno 115 EINPROGRESS gets thrown here - the socket is blocked. Why, and which socket, I do not know.
			// TODO instructions must be fiddled to allow packet-ins to correctly traverse the proxy
			std::cout << func << ": About to send_flow_mod_message." << std::endl;
			send_flow_mod_message( m_slave, match_, msg->get_cookie(), msg->get_cookie_mask(), msg->get_table_id(), msg->get_command(), msg->get_idle_timeout(), \
				msg->get_hard_timeout(), msg->get_priority(), msg->get_buffer_id(), msg->get_out_port(), msg->get_out_group(), msg->get_flags(), instructions_ );
				std::cout << func << ": send_flow_mod_message called at " << m_slave->c_str() << "." << std::endl;
		}
#endif
//		rofl::cflowentry entry(m_slave->get_version());
///		rofl::cflowentry entry(PROXYOFPVERSION);

///		BOOST_PP_SEQ_FOR_EACH( CLONECMD, ((*msg))(entry), (command)(table_id)(idle_timeout)(hard_timeout)(cookie)(cookie_mask)(priority)(buffer_id)(out_port)(out_group)(flags) )
///		entry.match = msg->get_match();
/*		fe.set_command(OFPFC_ADD);
		fe.set_buffer_id(msg->get_buffer_id());
		fe.set_idle_timeout(15);
		fe.set_table_id(msg->get_table_id());

		fe.match.set_in_port(msg->get_match().get_in_port());
		fe.match.set_eth_dst(msg->get_packet().ether()->get_dl_dst());
		fe.instructions.next() = cofinst_apply_actions(dpt->get_version()); */
		// valid for OFP10_VERSION only
///		rofl::cofinlist inl;
		const char * OF10_CMD_NAMES [] = { "OFPFC_ADD", "OFPFC_MODIFY", "OFPFC_MODIFY_STRICT", "OFPFC_DELETE", "OFPFC_DELETE_STRICT" };
		std::cout << func << ": msg: command:" << (int) msg->get_command() << "(" << OF10_CMD_NAMES[msg->get_command()] << ") cookie:" << msg->get_cookie() << " flags:" << msg->get_flags() << std::endl;
//		rofl::cflowentry entry(m_slave->get_version());
		rofl::cofinst_apply_actions cofac(OFP10_VERSION);
		std::cout << "TP-4" << std::endl;
		cofac.actions = msg->get_actions();
		std::cout << "TP-3" << std::endl;
		rofl::cofinlist instructions_(OFP10_VERSION);
		std::cout << "TP-2" << std::endl;
		instructions_.next() = cofac;
		std::cout << "TP-1" << std::endl;
		rofl::cofmatch match_(msg->get_match());
		std::cout << "TP0" << std::endl;
		uint64_t cookie_;
		uint64_t cookie_mask_ = 0;
		uint8_t table_id_;
		uint8_t command_;
		uint16_t idle_timeout_;
		uint16_t hard_timeout_;
		uint16_t priority_;
		uint32_t buffer_id_;
		uint32_t out_port_;
		uint32_t out_group_ = 0;
		uint16_t flags_;

		switch(msg->get_command()) {
			case OFPFC_ADD:
				assert(false);
/*				entry.set_command(OFPFC_ADD);
				entry.set_buffer_id(msg->get_buffer_id());
//				rofl::cofmatch match_(msg->get_match());
				entry.set_idle_timeout(15);
				entry.set_table_id(msg->get_table_id());
//				entry.match.set_in_port(msg->get_match().get_in_port());
//				entry.match.set_eth_dst(msg->get_packet().ether()->get_dl_dst());
				entry.match = msg->get_match();
				entry.instructions.next() = cofinst_apply_actions(OFP10_VERSION);*/
				break;
			case OFPFC_DELETE:
/*				std::cout << "TP1" << std::endl;
				entry.set_command(OFPFC_DELETE);
				std::cout << "TP2" << std::endl;
				BOOST_PP_SEQ_FOR_EACH( CLONECMD, ((*msg))(entry), (table_id)(idle_timeout)(hard_timeout)(cookie)(cookie_mask)(priority)(buffer_id)(out_port)(out_group)(flags) )
				std::cout << "TP3" << std::endl;
				entry.match = match_;
				std::cout << "TP4" << std::endl;
				cofac.actions = msg->get_actions();	
				std::cout << "TP5" << std::endl;
				entry.instructions.next() = cofac;*/
				std::cout << "TP6" << std::endl;
				cookie_ = msg->get_cookie();
				std::cout << "TP6a" << std::endl;
//				cookie_mask_ = msg->get_cookie_mask();	// only OFP12 and OFP13
				std::cout << "TP6b" << std::endl;
				table_id_ = msg->get_table_id();
				std::cout << "TP6c" << std::endl;
				command_ = msg->get_command();
				std::cout << "TP6d" << std::endl;
				idle_timeout_ = msg->get_idle_timeout();
				std::cout << "TP6e" << std::endl;
				hard_timeout_ = msg->get_hard_timeout();
				std::cout << "TP6f" << std::endl;
				priority_ = msg->get_priority();
				std::cout << "TP6g" << std::endl;
				buffer_id_ = msg->get_buffer_id();
				std::cout << "TP6h" << std::endl;
				out_port_ = msg->get_out_port();
				std::cout << "TP6i" << std::endl;
//				out_group_ = msg->get_out_group();		// only OFP12 and OFP13
				std::cout << "TP6j" << std::endl;
				flags_ = msg->get_flags();
				std::cout << "TP_" << std::endl;
				send_flow_mod_message( m_slave, match_, cookie_, cookie_mask_, table_id_, command_, idle_timeout_, hard_timeout_, priority_, buffer_id_, out_port_, out_group_, flags_, instructions_ );
				std::cout << "TP__" << std::endl;
				break;
			case OFPFC_DELETE_STRICT:
				assert(false);
				break;
			case OFPFC_MODIFY:
				assert(false);
				break;
			case OFPFC_MODIFY_STRICT:
				assert(false);
				break;
			default:
				std::cout << func << ": got unknown action." << std::endl;
				break;
		}
	/*
//		rofl::cofaclist acl(msg->get_actions());
		for(rofl::cofaclist::iterator cit=acl.begin(); cit != acl.end(); ++cit) {
			const rofl::cofaction act = *cit;
			std::cout << func << ": got action: " << cit->c_str() << "." << std::endl;
//			inl.next() = ??;
		}
	*/
//		entry.instructions()

//		send_flow_mod_message(m_slave, entry);

	} catch (rofl::eBadVersion &e) {
		std::cout << func << ": eBadVersion caught " << e << ". at " << __FILE__ << ":" << __LINE__ << std::endl;
	} catch (rofl::cerror &e) {
		std::cout << func << ": caught " << e << ". at " << __FILE__ << ":" << __LINE__ << std::endl;
	}
	// TODO what about get_xid() and get_actions() in cofmsg_flow_mod ? get_actions() may be  OFP10 only - at least according to cofmsg_flow_mod ctor
	// TODO perhaps use xid as key to map where a list of flow mods are stored? so we have a local record of flow mods
	
	delete(msg);
}
#endif



