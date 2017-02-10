#include <algorithm>
#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <map>

using std::string;
using std::cin;
using std::cout;
using std::endl;
using std::map;

#include "derecho/derecho_group.h"
#include "derecho/derecho_caller.h"
#include "block_size.h"
#include "derecho/managed_group.h"
#include "derecho/view.h"

void query_node_info(derecho::node_id_t &node_id, derecho::ip_addr &node_ip, derecho::ip_addr &leader_ip) {
    cout << "Please enter this node's ID: ";
    cin >> node_id;
    cout << "Please enter this node's IP address: ";
    cin >> node_ip;
    cout << "Please enter the leader node's IP address: ";
    cin >> leader_ip;
}

int main(int argc, char *argv[]) {
    try {
        if(argc < 2) {
            cout << "Error: Expected number of nodes in experiment as the first argument."
                 << endl;
            return -1;
        }
        uint32_t num_nodes = std::atoi(argv[1]);
        derecho::node_id_t node_id;
        derecho::ip_addr my_ip;
        derecho::node_id_t leader_id = 0;
        derecho::ip_addr leader_ip;

        query_node_info(node_id, my_ip, leader_ip);

        long long unsigned int max_msg_size = 1000000;
        long long unsigned int block_size = 100000;

        int num_messages = 10;
        int received_count = 0;

        bool done = false;
        auto stability_callback = [&num_messages, &done, &received_count](
            int sender_rank, long long int index, char *buf,
            long long int msg_size) {
            received_count++;
            cout << "In stability callback; sender = " << sender_rank
                 << ", index = " << index << endl;
            if(received_count == num_messages) {
                done = true;
            }
        };

        derecho::CallbackSet callbacks{stability_callback, nullptr};
        derecho::DerechoParams param_object{max_msg_size, block_size};
        Dispatcher<> empty_dispatcher(node_id);
        std::unique_ptr<derecho::ManagedGroup<Dispatcher<>>> managed_group;

        derecho::SubgroupInfo subgroup_info{[](uint32_t num_members) {
	    if (num_members >= 3) {
	      return 2;
	    }
	    return 0;
        },
                                            [](uint32_t num_members, uint32_t subgroup_num) {
					      return 1;
                                            },
                                            [](uint32_t num_members, uint32_t subgroup_num, uint32_t shard_num) {
					      std::vector<uint32_t> members;
					      if (subgroup_num == 0) {
						members.push_back(0);
						members.push_back(1);
					      }
					      else {
						members.push_back(1);
						members.push_back(2);
					      }
					      return members;
                                            }};
        if(node_id == leader_id) {
            assert(my_ip == leader_ip);
            managed_group = std::make_unique<derecho::ManagedGroup<Dispatcher<>>>(
                my_ip, std::move(empty_dispatcher), callbacks, subgroup_info, param_object);
        } else {
            managed_group = std::make_unique<derecho::ManagedGroup<Dispatcher<>>>(
                node_id, my_ip, leader_id, leader_ip, std::move(empty_dispatcher), callbacks, subgroup_info);
        }

        cout << "Finished constructing/joining ManagedGroup" << endl;

        while(managed_group->get_members().size() < num_nodes) {
        }

        auto send = [&](uint32_t subgroup_num) {
	  for(int i = 0; i < num_messages; ++i) {
            // random message size between 1 and 100
            unsigned int msg_size = (rand() % 7 + 2) * (max_msg_size / 10);
            char *buf = managed_group->get_sendbuffer_ptr(subgroup_num, msg_size);
            //        cout << "After getting sendbuffer for message " << i <<
            //        endl;
            //        managed_group.debug_print_status();
            while(!buf) {
                buf = managed_group->get_sendbuffer_ptr(subgroup_num, msg_size);
            }
            for(unsigned int j = 0; j < msg_size; ++j) {
                buf[j] = 'a' + i;
            }
            //        cout << "Client telling DerechoGroup to send message " <<
            //        i << "
            //        with size " << msg_size << endl;;
            managed_group->send(subgroup_num);
	  } };
        if(node_id == 1) {
	  send(0);
	  send(1);
        }
	else if (node_id == 0){
	  send(0);
	}
	else if (node_id == 2) {
	  send(1);
	}
	
        while(!done) {
        }

	cout << "Done" << endl;
        managed_group->barrier_sync();

        managed_group->leave();

    } catch(const std::exception &e) {
        cout << "Main got an exception: " << e.what() << endl;
        throw e;
    }

    cout << "Finished destroying managed_group" << endl;
}
