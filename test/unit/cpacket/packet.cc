/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include <rofl/common/cmacaddr.h>
#include <rofl/common/cmemory.h>
#include <rofl/common/cpacket.h>
#include <stdlib.h>

using namespace rofl;

void check_parser();
void check_push_pop_vlan();
void check_push_pop_mpls();
void check_pbb();
void check_fpppoeframe();

cmemory create_ether_header(
		cmacaddr const& dst,
		cmacaddr const& src,
		uint16_t eth_type);

cmemory create_vlan_tag(
		uint16_t vid,
		uint8_t pcp,
		uint16_t eth_type);

cmemory create_mpls_tag(
		uint32_t label,
		uint8_t tc,
		uint8_t ttl,
		bool bos);

cmemory create_ipv4_header(
		uint8_t tos,
		uint16_t total_len,
		uint16_t ident,
		uint8_t ttl,
		uint8_t proto,
		caddress const& src,
		caddress const& dst);

cmemory create_payload(
		size_t size,
		uint8_t value);

//cmemory create_ipv4_header();




int
main(int args, char** argv)
{
	//check_parser();
#if 0
	check_push_pop_vlan();
	check_push_pop_mpls();
	check_pbb();
#endif

	return EXIT_SUCCESS;
}



// 0x88a8

cmemory
create_ether_header(
		cmacaddr const& dst,
		cmacaddr const& src,
		uint16_t eth_type)
{
	cmemory ether(14); 							// empty ethernet header

	memcpy(ether.somem(), dst.somem(), OFP_ETH_ALEN);
	memcpy(ether.somem() + OFP_ETH_ALEN, src.somem(), OFP_ETH_ALEN);

	ether[12] = (eth_type & 0xff00) >> 8;
	ether[13] = (eth_type & 0x00ff);

	return ether;
}



cmemory
create_vlan_tag(
		uint16_t vid,
		uint8_t pcp,
		uint16_t eth_type)
{
	cmemory vlan(4);

	vlan[1] 	 = vid & 0x00ff;
	vlan[0] 	&= 0xf0;
	vlan[0]		 = ((pcp & 0x07) << 5) | ((vid & 0x0f00) >> 8);

	vlan[2] 	 = (eth_type & 0xff00) >> 8;
	vlan[3]		 = (eth_type & 0x00ff);

	return vlan;
}



cmemory
create_mpls_tag(
		uint32_t label,
		uint8_t tc,
		uint8_t ttl,
		bool bos)
{
	cmemory mpls(4);

	mpls[0] =  (label & 0x000ff000) >> 12;
	mpls[1] =  (label & 0x00000ff0) >>  4;
	mpls[2] = ((label & 0x0000000f) <<  4) + ((tc & 0x07) << 1) + (bos & 0x01);
	mpls[3] = ttl;

	return mpls;
}




cmemory
create_ipv4_header(
		uint8_t tos,
		uint16_t total_len,
		uint16_t ident,
		uint8_t ttl,
		uint8_t proto,
		caddress const& src,
		caddress const& dst)
{
	cmemory ipv4(20);

	ipv4[0] = (4 << 4) + 5;
	ipv4[1] = tos;
	ipv4[2] = (total_len & 0xff00) >> 8;
	ipv4[3] = (total_len & 0x00ff) >> 0;
	ipv4[4] = (ident & 0xff00) >> 8;
	ipv4[5] = (ident & 0x00ff) >> 0;
	ipv4[6] = 0x88; // flags + fragment offset
	ipv4[7] = 0x99; // fragment offset
	ipv4[8] = ttl;
	ipv4[9] = proto;
	ipv4[10] = 0xaf; // header checksum
	ipv4[11] = 0xfa; // header checksum
	memcpy(&ipv4[12], &(src.ca_s4addr->sin_addr.s_addr), sizeof(uint32_t));
	memcpy(&ipv4[16], &(dst.ca_s4addr->sin_addr.s_addr), sizeof(uint32_t));

	return ipv4;
}



cmemory
create_payload(size_t size, uint8_t value)
{
	cmemory payload(size);

	for (unsigned int i = 0; i < size; i++)
	{
		payload[i] = value;
	}
	return payload;
}



void
check_push_pop_vlan()
{
	cmemory start(0);

	start += create_ether_header(
			cmacaddr("00:11:11:11:11:11"),
			cmacaddr("00:22:22:22:22:22"),
			0x0800 /*IPv4*/);
	start += create_ipv4_header(0x00, 38, 0x4444, 0x10, 0x00, caddress(AF_INET, "10.1.1.1"), caddress(AF_INET, "10.2.2.2"));
	start += create_payload(18, 0x80);



	/*
	 * sub-test-1 => push vlan and set fields, pop vlan again
	 */
	{
		cmemory result1(0);

		result1 += create_ether_header(
				cmacaddr("00:11:11:11:11:11"),
				cmacaddr("00:22:22:22:22:22"),
				0x8100 /*IEEE802.1ad*/);
		result1 += create_vlan_tag(0xfff, 0xaa, 0x0800);
		result1 += create_ipv4_header(0x00, 38, 0x4444, 0x10, 0x00, caddress(AF_INET, "10.1.1.1"), caddress(AF_INET, "10.2.2.2"));
		result1 += create_payload(18, 0x80);


		cpacket a1(start.somem(), start.memlen());



		printf("push vlan tag => 0x8100, 0xfff, 0xaa ...");
		a1.push_vlan(0x8100);
		a1.set_field(coxmatch_ofb_vlan_vid(0xfff));
		a1.set_field(coxmatch_ofb_vlan_pcp(0xaa));

		if (a1 != result1)
		{
			std::cerr << "push_vlan failed =>" << std::endl;
			std::cerr << "expected: " << result1 << std::endl;
			std::cerr << "received: " << a1 << std::endl;

			exit(EXIT_FAILURE);
		}
		else
		{
			printf("success.\n");
		}

		printf("pop vlan tag ...");
		a1.pop_vlan();

		if (a1 != start)
		{
			std::cerr << " failed =>" << std::endl;
			std::cerr << "expected: " << start << std::endl;
			std::cerr << "received: " << a1 << std::endl;

			exit(EXIT_FAILURE);
		}
		else
		{
			std::cerr << "success." << std::endl;
		}
	}





	/*
	 * sub-test-2 => push two vlans and set fields, pop two vlans again
	 */
	{
		cmemory result1(0);

		result1 += create_ether_header(
				cmacaddr("00:11:11:11:11:11"),
				cmacaddr("00:22:22:22:22:22"),
				0x88a8 /*IEEE802.1ad Q-in-Q*/);
		result1 += create_vlan_tag(0xfff, 0xaa, 0x8100);
		result1 += create_vlan_tag(0xeee, 0xbb, 0x0800);
		result1 += create_ipv4_header(0x00, 38, 0x4444, 0x10, 0x00, caddress(AF_INET, "10.1.1.1"), caddress(AF_INET, "10.2.2.2"));
		result1 += create_payload(18, 0x80);




		cpacket a1(start.somem(), start.memlen(), /*in_port=*/3, true);



		printf("push vlans for Q-in-Q test => (0x88a8, 0xfff, 0xaa) / (0x8100, 0xeee, 0xbb) ...");

		a1.push_vlan(0x8100);
		a1.set_field(coxmatch_ofb_vlan_vid(0xeee));
		a1.set_field(coxmatch_ofb_vlan_pcp(0xbb));

		a1.push_vlan(0x88a8);
		a1.set_field(coxmatch_ofb_vlan_vid(0xfff));
		a1.set_field(coxmatch_ofb_vlan_pcp(0xaa));

		if (not (a1 == result1))
		{
			std::cerr << "failed =>" << std::endl;
			std::cerr << "expected: " << result1 << std::endl;
			std::cerr << "received: " << a1 << std::endl;

			exit(EXIT_FAILURE);
		}
		else
		{
			std::cerr << "success." << std::endl;
		}

		printf("pop vlans for Q-in-Q test ...");
		a1.pop_vlan();
		a1.pop_vlan();

		if (not (a1 == start))
		{
			std::cerr << "failed =>" << std::endl;
			std::cerr << "expected: " << start << std::endl;
			std::cerr << "received: " << a1 << std::endl;

			exit(EXIT_FAILURE);
		}
		else
		{
			printf("success.\n");
		}
	}
}



void
check_push_pop_mpls()
{
	cmemory pack(0);

	pack += create_ether_header(
			cmacaddr("00:11:11:11:11:11"),
			cmacaddr("00:22:22:22:22:22"),
			0x0800 /*IPv4*/);
	pack += create_ipv4_header(0x00, 38, 0x4444, 0x10, 0x00, caddress(AF_INET, "10.1.1.1"), caddress(AF_INET, "10.2.2.2"));
	pack += create_payload(18, 0x80);




	/*
	 * sub-test-1 => push mpls tag and set fields, pop mpls tag again
	 */
	{
		cmemory result1(0);

		result1 += create_ether_header(
				cmacaddr("00:11:11:11:11:11"),
				cmacaddr("00:22:22:22:22:22"),
				0x8848 /*MPLS*/);
		result1 += create_mpls_tag(0x55555, 0x7, 0x20, true /*bos*/);
		result1 += create_ipv4_header(0x00, 38, 0x4444, 0x10, 0x00, caddress(AF_INET, "10.1.1.1"), caddress(AF_INET, "10.2.2.2"));
		result1 += create_payload(18, 0x80);




		cpacket a1(pack.somem(), pack.memlen(), /*in_port=*/3, true);



		printf("push mpls tag => label: 0x55555, tc: 0x7, ttl: 0x20, bos: 1 ...");
		a1.push_mpls(0x8848);
		a1.set_field(coxmatch_ofb_mpls_label(0x55555));
		a1.set_field(coxmatch_ofb_mpls_tc(0x7));
		a1.set_mpls_ttl(0x20);

		if (a1 != result1)
		{
			std::cerr << "push_mpls failed =>" << std::endl;
			std::cerr << "expected: " << result1 << std::endl;
			std::cerr << "received: " << a1 << std::endl;

			exit(EXIT_FAILURE);
		}
		else
		{
			std::cerr << "success." << std::endl;
		}




		std::cerr << "pop mpls tag ...";
		a1.pop_mpls(0x0800);

		if (a1 != pack)
		{
			std::cerr << "push_mpls failed =>" << std::endl;
			std::cerr << "expected: " << pack << std::endl;
			std::cerr << "received: " << a1 << std::endl;

			exit(EXIT_FAILURE);
		}
		else
		{
			std::cerr << "success." << std::endl;
		}
	}





	/*
	 * sub-test-2 => push two mpls tags and set fields, pop two mpls tags again
	 */
	{
		cmemory result1(0);

		result1 += create_ether_header(
				cmacaddr("00:11:11:11:11:11"),
				cmacaddr("00:22:22:22:22:22"),
				0x8848 /*MPLS*/);
		result1 += create_mpls_tag(0x55555, 0x7, 0x20, false /*bos*/); // outer tag
		result1 += create_mpls_tag(0x77777, 0x3, 0x10, true /*bos*/); // inner tag
		result1 += create_ipv4_header(0x00, 38, 0x4444, 0x10, 0x00, caddress(AF_INET, "10.1.1.1"), caddress(AF_INET, "10.2.2.2"));
		result1 += create_payload(18, 0x80);




		cpacket a1(pack.somem(), pack.memlen(), /*in_port=*/3, true);



		printf("push mpls tags for label stack test => (0x55555, 0x7, 0x20) / (0x77777, 0x3, 0x10) ...");

		a1.push_mpls(0x8848);
		a1.set_field(coxmatch_ofb_mpls_label(0x77777));
		a1.set_field(coxmatch_ofb_mpls_tc(0x3));
		a1.set_mpls_ttl(0x10);

		a1.push_mpls(0x8848);
		a1.set_field(coxmatch_ofb_mpls_label(0x55555));
		a1.set_field(coxmatch_ofb_mpls_tc(0x7));
		a1.set_mpls_ttl(0x20);


		if (a1 != result1)
		{
			std::cerr << "push_vlan failed =>" << std::endl;
			std::cerr << "expected: " << result1 << std::endl;
			std::cerr << "received: " << a1 << std::endl;

			exit(EXIT_FAILURE);
		}
		else
		{
			std::cerr << "success." << std::endl;
		}

		printf("pop mpls tags for label stack test ...");
		a1.pop_mpls(0x8848);
		a1.pop_mpls(0x0800);

		if (a1 != pack)
		{
			std::cerr << "push_mplsn failed =>" << std::endl;
			std::cerr << "expected: " << pack << std::endl;
			std::cerr << "received: " << a1 << std::endl;

			exit(EXIT_FAILURE);
		}
		else
		{
			std::cerr << "success." << std::endl;
		}
	}
}







void
check_parser()
{
	cmemory ptest(0);

	ptest += create_ether_header(
			cmacaddr("00:11:11:11:11:11"),
			cmacaddr("00:22:22:22:22:22"),
			0x88a8 /*IEEE802.1ad Q-in-Q*/);
	ptest += create_vlan_tag(0xfff, 0x0, 0x8100);
	ptest += create_vlan_tag(0x333, 0x7, 0x8848);
	ptest += create_mpls_tag(0x55555, 0x03, 0x10, false);
	ptest += create_mpls_tag(0x77777, 0x00, 0x20, true);
	ptest += create_ipv4_header(0x00, 38, 0x4444, 0x10, 0x00, caddress(AF_INET, "10.1.1.1"), caddress(AF_INET, "10.2.2.2"));


	std::cerr << "cpacket::classify() test packet: " << ptest << std::endl;


	cpacket a1(ptest.somem(), ptest.memlen(), /*in_port=*/3, true);

	std::cerr << "a1: " << a1 << std::endl;


	std::cerr << "pop outer mpls => should be ignored" << std::endl;
	a1.pop_mpls(0x8847);
	std::cerr << "a1: " << a1 << std::endl;


	std::cerr << "pop outer vlan =>" << std::endl;
	a1.pop_vlan();
	std::cerr << "a1: " << a1 << std::endl;


	return;

#if 0
	if (a1 != a2)
	{
		exit(EXIT_FAILURE);
	}
	else
	{
		printf("cmemory::operator[] () success\n");
	}
#endif
}



