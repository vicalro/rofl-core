#include "cofmsg_echo.h"

using namespace rofl;

cofmsg_echo::cofmsg_echo(
		uint8_t of_version,
		uint8_t type,
		uint32_t xid,
		uint8_t* data,
		size_t datalen) :
	cofmsg(sizeof(struct ofp_header)),
	body(0)
{
	body.assign(data, datalen);

	set_version(of_version);
	set_length(sizeof(struct ofp_header));
	set_type(type);
	set_xid(xid);
}



cofmsg_echo::cofmsg_echo(
		cmemory *memarea) :
	cofmsg(memarea),
	body(0)
{

}



cofmsg_echo::cofmsg_echo(
		cofmsg_echo const& echo)
{
	*this = echo;
}



cofmsg_echo&
cofmsg_echo::operator= (
		cofmsg_echo const& echo)
{
	if (this == &echo)
		return *this;

	cofmsg::operator =(echo);

	body = echo.body;

	return *this;
}



cofmsg_echo::~cofmsg_echo()
{

}



void
cofmsg_echo::reset()
{
	cofmsg::reset();
	body.clear();
}



size_t
cofmsg_echo::length() const
{
	return (sizeof(struct ofp_header) + body.memlen());
}



void
cofmsg_echo::pack(uint8_t *buf, size_t buflen)
{
	set_length(length());

	if ((0 == buf) || (buflen == 0))
		return;

	if (buflen < length())
		throw eInval();

	cofmsg::pack(buf, buflen);

	memcpy(buf + sizeof(struct ofp_header), body.somem(), body.memlen());
}



void
cofmsg_echo::unpack(uint8_t *buf, size_t buflen)
{
	cofmsg::unpack(buf, buflen);

	validate();
}



void
cofmsg_echo::validate()
{
	cofmsg::validate(); // check generic OpenFlow header

	switch (get_version()) {
	case OFP10_VERSION:
	case OFP12_VERSION:
	case OFP13_VERSION: {
		if (get_length() > sizeof(struct ofp_header)) {
			body.assign(sobody(), bodylen());
		}
	} break;
	default:
		throw eBadRequestBadVersion();
	}
}



cmemory&
cofmsg_echo::get_body()
{
	return body;
}

