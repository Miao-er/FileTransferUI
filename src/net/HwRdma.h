#ifndef HW_RDMA_H
#define HW_RDMA_H

#include <infiniband/verbs.h>
#include <iostream>
#include <vector>
#include <map>
#include <mutex>
using std::cout, std::endl;
class HwRdma
{
public:
    HwRdma(int gid_idx, int port_num, uint64_t buffer_size)
    {
        this->gid_idx = gid_idx;
        this->port_num = port_num;
        this->buffer_size = buffer_size;
        this->free_size = buffer_size;
    }
    int setFreeSize(uint64_t size)
    {
        this->free_size = size;
    }
    int init()
    {
        cout << "Looking for IB devices ..." << endl;
        int num_devices = 0;
        struct ibv_device **devs = ibv_get_device_list(&num_devices);

        // List devices
        cout << endl
             << "=============================================" << endl;
        cout << "Found " << num_devices << " devices" << endl;
        cout << "---------------------------------------------" << endl;
        for (int i = 0; i < num_devices; i++)
        {

            const char *transport_type = "unknown";
            switch (devs[i]->transport_type)
            {
            case IBV_TRANSPORT_IB:
                transport_type = "IB";
                break;
            case IBV_TRANSPORT_IWARP:
                transport_type = "IWARP";
                break;
            case IBV_TRANSPORT_USNIC:
                transport_type = "USNIC";
                break;
            default:
                transport_type = "UNKNOWN";
                break;
            }
            uint64_t lid = 0;
             // Open device
            struct ibv_context *local_ctx = ibv_open_device(devs[i]);
            struct ibv_port_attr local_port_attr;

            if (local_ctx)
            {
                auto ret = ibv_query_port(local_ctx, this->port_num, &local_port_attr);
                if (this->dev == nullptr && ret == 0)
                {
                    this->dev = devs[i];
                }
                ibv_close_device(local_ctx);

            }
            // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

            cout << "   device " << i
                 << " : " << devs[i]->name
                 << " : " << devs[i]->dev_name
                 << " : " << transport_type
                 << " : " << ibv_node_type_str(devs[i]->node_type)
                 << endl;
        }
        cout << "=============================================" << endl
             << endl;

        // Open device
        this->ctx = ibv_open_device(this->dev);
        ibv_free_device_list(devs);
        if (!this->ctx)
        {
            cout << "ERROR: opening IB device context!" << endl;
            return -1;
        }
        // Get device and port attributes
        ibv_query_device(this->ctx, &this->attr);
        ibv_query_port(this->ctx, this->port_num, &this->port_attr);
        ibv_query_gid(this->ctx, this->port_num, this->gid_idx, &this->gid);

        cout << "Device " << dev->name << " opened."
             << " num_comp_vectors=" << ctx->num_comp_vectors
             << endl;

        // Print some of the port attributes
        cout << "Port attributes:" << endl;
        cout << "           state: " << port_attr.state << endl;
        cout << "         max_mtu: " << port_attr.max_mtu << endl;
        cout << "      active_mtu: " << port_attr.active_mtu << endl;
        cout << "  port_cap_flags: " << port_attr.port_cap_flags << endl;
        cout << "      max_msg_sz: " << port_attr.max_msg_sz << endl;
        cout << "    active_width: " << (uint64_t)port_attr.active_width << endl;
        cout << "    active_speed: " << (uint64_t)port_attr.active_speed << endl;
        cout << "      phys_state: " << (uint64_t)port_attr.phys_state << endl;
        cout << "      link_layer: " << (uint64_t)port_attr.link_layer << endl;

        // Allocate protection domain
        this->pd = ibv_alloc_pd(this->ctx);
        if (!this->pd)
        {
            cout << "ERROR: allocation protection domain!" << endl;
            return -1;
        }
        return 0;
    }
    int create_mr(struct ibv_mr **mr, uint8_t **buffer_ptr, size_t length)
    {
        if(length <= 0)
        {
            cout << "ERROR: wrong size expected to create mr." <<endl;
            return -1;
        }
        //for client, init only one mr dynamically
        if(this->free_size == uint64_t(-1))
            this->free_size = length;
        if (this->free_size < length)
        {
            cout << "ERROR: the remain free space is not enough!" << endl;
            return -1;
        }
        *buffer_ptr = new uint8_t[length];
        if (!(*buffer_ptr))
        {
            cout << "ERROR: Unable to allocate buffer!" << endl;
            return -1;
        }
        auto access = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE;
        *mr = ibv_reg_mr(pd, *buffer_ptr, length, access);
        if (!(*mr))
        {
            cout << "ERROR: Unable to register memory region!" << endl;
            delete[] (*buffer_ptr);
            return -1;
        }
        std::lock_guard<std::mutex> lock(this->mr_mutex);
        this->free_size -= length;
        mr_set.insert(std::make_pair((uint64_t)(*buffer_ptr), *mr));

        return 0;
    }
    int destroy_mr(struct ibv_mr* mr)
    {
        std::lock_guard<std::mutex> lock(this->mr_mutex);
        for(auto it = mr_set.begin();it != mr_set.end(); ++it)
        {
            if(it->second == mr){
                delete[] (uint8_t*)(it->first);
                this->free_size += it->second->length;
                ibv_dereg_mr(it->second);
                mr_set.erase(it);  
                return 0;
                break;
            }
        }
        return -1;
    }
    ~HwRdma()
    {
        for(auto it = mr_set.begin();it != mr_set.end(); ++it)
        {
            delete[] (uint8_t*)(it->first);
            ibv_dereg_mr(it->second);
        }
        if (pd != nullptr)
            ibv_dealloc_pd(pd);
        if (ctx != nullptr)
            ibv_close_device(ctx);
    }
    // device
    struct ibv_device *dev = nullptr;
    struct ibv_device_attr attr;
    struct ibv_context *ctx = nullptr;
    // port
    int port_num;
    struct ibv_port_attr port_attr;
    // pd
    struct ibv_pd *pd = nullptr;
    // mr & buffer
    std::map<uint64_t, ibv_mr *> mr_set;
    uint64_t buffer_size;
    uint64_t free_size;
    std::mutex mr_mutex;
    // gid
    int gid_idx;
    ibv_gid gid;
};

#endif