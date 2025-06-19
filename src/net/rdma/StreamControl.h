#ifndef STREAM_CONTROL_H
#define STREAM_CONTROL_H

#include <infiniband/verbs.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <tuple>
#include <vector>
#include <list>
#include <iostream>
#include <errno.h>
#include <exception>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <fstream>
#include "HwRdma.h"
#include "../../utils/LocalConf.h"

using std::chrono::duration_cast;

struct QPInfo
{
    uint16_t lid;
    // uint16_t lucp_id;
    uint32_t qp_num;
    uint32_t block_num;
    uint32_t block_size;
    uint8_t gid[16];
} __attribute__((packed));

struct FileInfo
{
    char file_path[256];
    uint64_t file_size;
} __attribute__((packed));


class StreamControl
{
private:
    uint8_t *buf_ptr = nullptr;
    struct ibv_mr *mr = nullptr;
    double default_rate;
    uint32_t block_size;
    HwRdma *hwrdma;
    int peer_fd;
    std::vector<std::tuple<uint8_t *, uint32_t>> buffers;

    struct ibv_comp_channel *comp_channel = nullptr;
    struct ibv_cq *cq = nullptr;
    struct ibv_qp *qp = nullptr;
    QPInfo local_qp_info, remote_qp_info;
    ClientList *client_list = nullptr;
    LocalConf *local_conf = nullptr;

public:

    StreamControl(HwRdma *hwrdma, int peer_fd, LocalConf *local_conf, ClientList *client_list)
    {
        this->hwrdma = hwrdma;
        this->peer_fd = peer_fd;
        this->default_rate = local_conf->getDefaultRate();
        this->local_conf = local_conf;
        this->client_list = client_list;
    }
    ~StreamControl()
    {
        if (qp != nullptr)
        {
            struct ibv_qp_attr qp_attr;
            bzero(&qp_attr, sizeof(qp_attr));
            qp_attr.qp_state = IBV_QPS_RESET;
            ibv_modify_qp(qp, &qp_attr, IBV_QP_STATE);
        }
        if (qp != nullptr)
            ibv_destroy_qp(qp);
        if (cq != nullptr)
            ibv_destroy_cq(cq);
        if (comp_channel != nullptr)
            ibv_destroy_comp_channel(comp_channel);
        if (mr != nullptr)
            hwrdma->destroy_mr(mr);
    }

    int bindMemoryRegion()
    {
        size_t length = this->block_size * local_conf->getBlockNum() * 1024;
        if(hwrdma->create_mr(&this->mr,&this->buf_ptr,length))
        {
            return -1;
        }
        return 0;
    }
    int createBufferPool()
    {
        if (!this->buf_ptr || ! this->mr)
        {
            cout << "ERROR: NULL memory region." << endl;
            return -1;
        }
        uint64_t loc = 0;

        while (loc + this->block_size <= this->mr->length)
        {
            buffers.emplace_back((uint8_t *)buf_ptr + loc, this->block_size);
            loc += this->block_size;
        }
        return 0;
    }
    int createLucpContext()
    {
        // create cp_channel
        comp_channel = ibv_create_comp_channel(hwrdma->ctx);
        // create cq
        cq = ibv_create_cq(hwrdma->ctx, buffers.size(), NULL, comp_channel, 0);
        if (!cq)
        {
            cout << "ERROR: Unable to create Completion Queue" << endl;
            return -1;
        }
        // create qp
        struct ibv_qp_init_attr qp_init_attr;
        bzero(&qp_init_attr, sizeof(qp_init_attr));
        qp_init_attr.send_cq = cq;
        qp_init_attr.recv_cq = cq;
        qp_init_attr.cap.max_send_wr = buffers.size();
        qp_init_attr.cap.max_recv_wr = buffers.size();
        qp_init_attr.cap.max_send_sge = 1;
        qp_init_attr.cap.max_recv_sge = 1;
        qp_init_attr.qp_type = IBV_QPT_RC;
        
        local_qp_info.lid = hwrdma->port_attr.lid;
        local_qp_info.block_num = local_conf->getBlockNum();
        local_qp_info.block_size = local_conf->getBlockSize();
        //local_qp_info.lucp_id = duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count(); //TODO
        // local_qp_info.recv_depth = qp_init_attr.cap.max_recv_wr; //must before create qp,or max_recv_wr will change.
        memcpy(local_qp_info.gid, &hwrdma->gid, 16);
        // Create Queue Pair
        qp = ibv_create_qp(hwrdma->pd, &qp_init_attr);
        if (!qp)
        {
            cout << "ERROR: Unable to create QP!" << endl;
            return -1;
        }
        local_qp_info.qp_num = qp->qp_num;
        return 0;
    }

    int sockSyncData(int xfer_size, char *local_data, char *remote_data)
    {
        int rc;
        int read_bytes = 0;
        int total_read_bytes = 0;
        rc = write(peer_fd, local_data, xfer_size);

        if (rc < xfer_size)
        {
            cout << "ERROR: Failed writing data during sock_sync_data." << endl;
            return -1;
        }
        else
            rc = 0;

        while (!rc && total_read_bytes < xfer_size)
        {
            read_bytes = read(peer_fd, remote_data, xfer_size);
            if (read_bytes > 0)
            {
                total_read_bytes += read_bytes;
            }
            else
            {
                rc = read_bytes;
            }
        }
        return rc;
    }
    int changeQPState()
    {
        /* Change QP state to INIT */
        {
            struct ibv_qp_attr qp_attr;
            bzero(&qp_attr, sizeof(qp_attr));
            qp_attr.qp_state = IBV_QPS_INIT,
            qp_attr.pkey_index = 0,
            qp_attr.port_num = hwrdma->port_num,
            qp_attr.qp_access_flags = IBV_ACCESS_LOCAL_WRITE |
                                      IBV_ACCESS_REMOTE_READ |
                                      IBV_ACCESS_REMOTE_ATOMIC |
                                      IBV_ACCESS_REMOTE_WRITE;

            auto ret = ibv_modify_qp(this->qp, &qp_attr,
                                     IBV_QP_STATE | IBV_QP_PKEY_INDEX |
                                         IBV_QP_PORT | IBV_QP_ACCESS_FLAGS);
            if (ret != 0)
            {
                cout << "ERROR: Unable to set QP to INIT state!" << endl;
                return ret;
            }
        }
        /* Change QP state to RTR */
        {
            struct ibv_qp_attr qp_attr;
            bzero(&qp_attr, sizeof(qp_attr));
            qp_attr.qp_state = IBV_QPS_RTR,
            qp_attr.path_mtu = hwrdma->port_attr.active_mtu,
            qp_attr.dest_qp_num = this->remote_qp_info.qp_num,
            qp_attr.rq_psn = 0,
            qp_attr.max_dest_rd_atomic = 1,
            qp_attr.min_rnr_timer = 0x12,
            // qp_attr.ah_attr.is_global  = 0,
            qp_attr.ah_attr.dlid = this->remote_qp_info.lid,
            qp_attr.ah_attr.sl = 0,
            qp_attr.ah_attr.src_path_bits = 0,
            qp_attr.ah_attr.port_num = hwrdma->port_num,

            qp_attr.ah_attr.is_global = 1,
            memcpy(&qp_attr.ah_attr.grh.dgid, remote_qp_info.gid, 16),
            qp_attr.ah_attr.grh.flow_label = 0,
            qp_attr.ah_attr.grh.hop_limit = 3, // TODO modify
                qp_attr.ah_attr.grh.sgid_index = hwrdma->gid_idx,
            qp_attr.ah_attr.grh.traffic_class = 0;

            auto ret = ibv_modify_qp(qp, &qp_attr,
                                     IBV_QP_STATE | IBV_QP_AV |
                                         IBV_QP_PATH_MTU | IBV_QP_DEST_QPN |
                                         IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC |
                                         IBV_QP_MIN_RNR_TIMER);
            if (ret != 0)
            {
                cout << "ERROR: Unable to set QP to RTR state!" << endl;
                return ret;
            }
        }

        /* Change QP state to RTS */
        {
            struct ibv_qp_attr qp_attr;
            bzero(&qp_attr, sizeof(qp_attr));
            qp_attr.qp_state = IBV_QPS_RTS,
            qp_attr.timeout = 18,
            qp_attr.retry_cnt = 7,
            qp_attr.rnr_retry = 0,
            qp_attr.sq_psn = 0,
            qp_attr.max_rd_atomic = 1;

            auto ret = ibv_modify_qp(qp, &qp_attr,
                                     IBV_QP_STATE | IBV_QP_TIMEOUT |
                                         IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY |
                                         IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC);
            if (ret != 0)
            {
                cout << "ERROR: Unable to set QP to RTS state!" << endl;
                return ret;
            }
        }
        return 0;
    }
    int connectPeer()
    {
        char remote_ready_char;
        if(sockSyncData(1,"R",&remote_ready_char))
        {
            cout << "ERROR: connect failed when sync ready info." << endl;
            return -1;
        }
        if(remote_ready_char != 'R')
        {
            cout << "ERROR: remote not ready to connect." << endl;
            return -1;
        }
        QPInfo net_local_qp_info, net_remote_qp_info;
        net_local_qp_info.lid = htons(local_qp_info.lid);
        //net_local_qp_info.lucp_id = htons(local_qp_info.lucp_id);
        net_local_qp_info.qp_num = htonl(local_qp_info.qp_num);
        net_local_qp_info.block_num = htonl(local_qp_info.block_num);
        net_local_qp_info.block_size = htonl(local_qp_info.block_size);
        //net_local_qp_info.recv_depth = htonl(local_qp_info.recv_depth);
        memcpy(net_local_qp_info.gid, local_qp_info.gid, 16);

        if (sockSyncData(sizeof(QPInfo), (char *)&net_local_qp_info, (char *)&net_remote_qp_info) < 0)
        {
            cout << "ERROR: connect failed when sync qpinfo." << endl;
            return -1;
        }
        remote_qp_info.lid = ntohs(net_remote_qp_info.lid);
        //remote_qp_info.lucp_id = ntohs(net_remote_qp_info.lucp_id);
        remote_qp_info.qp_num = ntohl(net_remote_qp_info.qp_num);
        remote_qp_info.block_num = ntohl(net_remote_qp_info.block_num);
        remote_qp_info.block_size = ntohl(net_remote_qp_info.block_size);
        //remote_qp_info.recv_depth = ntohl(net_remote_qp_info.recv_depth);
        memcpy(remote_qp_info.gid, net_remote_qp_info.gid, 16);
        //client apply block size from server
        if(this->client_list == nullptr)
            this->block_size = remote_qp_info.block_size;
#ifdef DEBUG
        cout << "     local:" << endl
        << "       lid:" << local_qp_info.lid << endl
        << "    qp_num:" << local_qp_info.qp_num << endl
        << " block_num:" << local_qp_info.block_num << endl
        << "block_size:" << local_qp_info.block_size << endl
        // << "recv_depth:" << local_qp_info.recv_depth << endl
        << "    remote:" << endl
        << "       lid:" << remote_qp_info.lid << endl
        << "    qp_num:" << remote_qp_info.qp_num << endl
        << " block_num:" << remote_qp_info.block_num << endl
        << "block_size:" << remote_qp_info.block_size << endl;
        // << "recv_depth:" << remote_qp_info.recv_depth << endl;
#endif
        return changeQPState();
    }

    int postRecvFile()
    {
        for(int i = 0; i < this->buffers.size(); i++)
            postRecvWr(i);

        FileInfo file_info, remote_file_info;
        memcpy(file_info.file_path, "READY_TO_RECEIVE",256);
        file_info.file_size = 0;
        if (sockSyncData(sizeof(file_info), (char *)&file_info, (char *)&remote_file_info) != 0)
        {
            cout << "ERROR: synchronous failed before post file." << endl;
            return -1;
        }
        local_conf->loadConf();
        string save_path = local_conf->getSavedFolderPath().ToStdString() + wxFileName::GetPathSeparator().ToStdString() + remote_file_info.file_path;
        int recv_fd = open(save_path.c_str(), O_CREAT|O_WRONLY, 0777);
        if (recv_fd < 0)
        {
            cout << "ERROR: Unable to create file \"" << save_path << "\"!"  << "errno = " << errno << endl;
            return -1;
        }
        std::shared_ptr<int> x(NULL, [&](int *){close(recv_fd);});
        uint64_t recv_bytes = 0;
        struct ibv_wc *wc = new ibv_wc[buffers.size()];
        auto t = high_resolution_clock::now();
        auto t_ = t;
        double delta_io = 0,delta = 0;
        int recv_num = 0;
        char sync_char = 'Y';
        sockSyncData(1, (char *)&sync_char, (char *)&sync_char);
        while(recv_bytes < remote_file_info.file_size)
        {
            int n = ibv_poll_cq(cq, 1, wc);
            if(n < 0)
            {
                cout << "ERROR: ibv_poll_cq returned " << n << " - closing connection";
                return -1;
            }
            else if(n == 0); //std::this_thread::sleep_for(std::chrono::microseconds(1));
            else{
                //cout << n << endl;
                for (int i = 0; i < n; i++)
                {
                    recv_num ++;
                    // auto delta_ = duration_cast<std::chrono::nanoseconds>(high_resolution_clock::now() - t_).count();
                    // t_ = high_resolution_clock::now();
                    // cout << delta_ << endl;
                    if (wc[i].status != IBV_WC_SUCCESS || wc[i].opcode != IBV_WC_RECV)
                    {
                        fprintf(stderr, "got bad completion with status: 0x%x, vendor syndrome: 0x%x\n",
                                wc[i].status, wc[i].vendor_err);
                    }
                    auto &buffer = buffers[wc[i].wr_id];
		            auto buff = std::get<0>(buffer);
                    recv_bytes += wc[i].byte_len;
                    //cout << "receive rate: " << wc[i].byte_len * 8 /(delta * 1e9) <<"Gbps" <<endl;
                    // auto io_start = high_resolution_clock::now();
                    write(recv_fd, (const char*)buff, wc[i].byte_len);
                    postRecvWr(wc[i].wr_id);
                    // auto io_end = high_resolution_clock::now();
                    // double delta_io_ = duration_cast<duration<double>>(io_end - io_start).count();
                    //cout << "write rate: " << wc[i].byte_len * 8 /(delta_io_ * 1e9) <<"Gbps" <<endl;
                    // delta_io += delta_io_;
                    // delta += delta_;
                    // if(recv_num > 0 && (recv_num % local_qp_info.recv_depth == 0)) //all wqe is in free state
                    send(this->peer_fd, "A", 1, MSG_DONTWAIT);
                }
            }
        }
        delta = duration_cast<duration<double>>(high_resolution_clock::now() - t).count();

        // cout << "I/O write rate: " << remote_file_info.file_size * 8/(delta_io * 1e9) << "Gbps" << endl;
        cout << "recv rate: " << remote_file_info.file_size * 8/(delta * 1e9) << "Gbps" << endl;
        cout << "finish receive file:" << remote_file_info.file_path << "(" << (double)remote_file_info.file_size/1e9 << "GB)" << endl;
        return 0;
    }
    int postSendFile(const char *file_path, const char *file_name)
    {
        struct stat statbuf;
        auto ret = stat(file_path, &statbuf);
        if (ret != 0)
        {
            cout << "ERROR: file not exist." << endl;
            return -1;
        }
        FileInfo file_info, remote_file_info;
        memcpy(file_info.file_path, file_name, 256);
        file_info.file_size = statbuf.st_size;
        if (sockSyncData(sizeof(file_info), (char *)&file_info, (char *)&remote_file_info) != 0)
        {
            cout << "ERROR: synchronous failed before post file." << endl;
            return -1;
        }
        if (strcmp(remote_file_info.file_path, "READY_TO_RECEIVE") != 0)
        {
            cout << "ERROR: remote not ready to receive." << endl;
            return -1;
        }
        int fd = open(file_path, O_RDONLY);
        if (fd < 0)
        {
            cout << "ERROR: Unable to open file \"" << file_path << "\"!" << endl;
            return -1;
        }

        double filesize_GB = (double)(file_info.file_size) * 1.0E-9;
        // cout << "Sending file: " << file_path << "(" << filesize_GB << " GB)" << endl;
        std::shared_ptr<int> x(NULL, [&](int *){close(fd);});
        struct ibv_send_wr wr, *bad_wr = nullptr;
        struct ibv_sge sge;
        bzero(&wr, sizeof(wr));
        bzero(&sge, sizeof(sge));
        wr.opcode = IBV_WR_SEND;
        wr.sg_list = &sge;
        wr.num_sge = 1;
        wr.send_flags = IBV_SEND_SIGNALED,
        wr.next = NULL,
        sge.lkey = this->mr->lkey;

        uint64_t bytes_left = file_info.file_size;
        uint32_t Noutstanding_writes = 0;
        double delta_t_io = 0.0;
        
        uint32_t sendcnt = 0, compcnt = 0;
        struct ibv_wc *wc = new ibv_wc[buffers.size()];
        std::list<high_resolution_clock::time_point> uncomplete_tp;
        auto t1 = high_resolution_clock::now();
        uint32_t i = 0;

        auto buff_size = std::get<1>(buffers[0]);
        sge.addr = (uint64_t)std::get<0>(buffers[0]);
        wr.wr_id = 0;
        auto bytes_payload = buff_size < bytes_left ? buff_size : bytes_left;
        sge.length = bytes_payload;

        // bool unread = false;
        auto t2 = high_resolution_clock::now();
        //for(; j < buffers.size() && buff_size * j < file_info.file_size;)
            //readahead(fd,(j++) * buff_size, buff_size);
        char sync_char = 'Y';
        sockSyncData(1, (char *)&sync_char, (char *)&sync_char);
        int remaining_recv_wqe = remote_qp_info.block_num;
        // std::ofstream fout("rtt.txt");
        // cout  << "start:" << duration_cast<std::chrono::nanoseconds>(high_resolution_clock::now().time_since_epoch()).count() << endl;
        while (1)
        { 
            // if (unread)
            {
                while(1)
                {
                    int nb = recv(this->peer_fd, &sync_char, 1, MSG_DONTWAIT);
                    if(nb == 0) return -1;
                    else if(nb < 0 && errno == EWOULDBLOCK)
                        break;
                    else if(nb < 0 && errno == EINTR)
                        continue;
                    else if(nb < 0)
                        return -1;
                    else 
                    {
                        assert(sync_char == 'A');
                        remaining_recv_wqe++;
                    }
                }

                // read(this->peer_fd, &sync_char, 1);
                // if(sync_char != 'A')
                // {
                //     cout << "ERROR: sync post/recv WQE error." << endl;
                // }
                // unread = false; //this cycle has synced.
            }
            //readahead(fd,file_info.file_size - bytes_left, sge.length);
            if(remaining_recv_wqe > 0)
            {
                // Calculate bytes to be sent in this buffer
                
                // if(i == 0)
                // {
                // auto t_io_start = high_resolution_clock::now();
                read(fd, (char *)sge.addr, bytes_payload);
                // auto t_io_end = high_resolution_clock::now();
                // duration<double> duration_io = duration_cast<duration<double>>(t_io_end - t_io_start);
                // }
                //cout << "read time: " << duration_io.count() << endl;
                // delta_t_io += duration_io.count();
                // t3 = high_resolution_clock::now();
                auto ret = ibv_post_send(qp, &wr, &bad_wr);

                if (ret != 0)
                {
                    cout << "ERROR: ibv_post_send returned non zero value (" << ret << ")" << endl;
                    break;
                }
                bytes_left -= bytes_payload;
                Noutstanding_writes++;
                remaining_recv_wqe --;
                sendcnt++;
                
                i++;
                auto id = i % buffers.size();
                auto &buffer = buffers[id];
                auto buff = std::get<0>(buffer);
                auto buff_size = std::get<1>(buffer);
                sge.addr = (uint64_t)buff;
                wr.wr_id = id;
                auto bytes_payload = buff_size < bytes_left ? buff_size : bytes_left;
                sge.length = bytes_payload;
                // if((i % this->remote_qp_info.recv_depth) == 0) unread = true; //wait for sync
            }
            
            do
            {
                int n = ibv_poll_cq(cq, buffers.size(), wc);
                // cout << Noutstanding_writes << endl;
                // cout << n << endl;
                if (n < 0)
                {
                    cout << "ERROR: ibv_poll_cq returned " << n << " - closing connection";
                    return -1;
                }
                else if (n > 0)
                {
                    for (int i = 0; i < n; i++)
                    {
                        if (wc[i].status != IBV_WC_SUCCESS)
                        {
                            fprintf(stderr, "got bad completion with status: 0x%x, vendor syndrome: 0x%x\n",
                                    wc[i].status, wc[i].vendor_err);
                            return -1;
                        }
                        //if(j * buff_size < file_info.file_size);
                            //readahead(fd,(j++) * buff_size, buff_size);

                        // auto send_time = uncomplete_tp.front();
                        // auto recv_time = high_resolution_clock::now();
                        // cout << "FIRST WQE RTT is " << duration_cast<duration<double>>(recv_time - send_time).count() << endl;
                        // uncomplete_tp.pop_front();
                        compcnt++;
                        Noutstanding_writes--;
                    }
                }
            } while (Noutstanding_writes >= buffers.size() || (bytes_left == 0 && Noutstanding_writes > 0));
            if(bytes_left == 0) break;
        }
        t2 = high_resolution_clock::now();
        delete[] wc;
        cout << endl;

        // Calculate total transfer rate and report.
        duration<double> delta_t = duration_cast<duration<double>>(t2 - t1);
        double rate_Gbps = (double)file_info.file_size/ delta_t.count() * 8.0 / 1.0E9;
        double rate_io_Gbps = (double)file_info.file_size / delta_t_io * 8.0 / 1.0E9;
        // cout << duration_cast<duration<double>>(t2 - t1).count() <<  "sec" << endl;
#ifndef DEBUG
        if (file_info.file_size > 2E8)
        {
            cout << "  Transferred " << (((double)file_info.file_size) * 1.0E-9) << " GB in " << delta_t.count() << " sec  (" << rate_Gbps << " Gbps)" << endl;
            // cout << "  I/O rate reading from file: " << delta_t_io << " sec  (" << rate_io_Gbps << " Gbps)" << endl;
        }
        else
        {
            cout << "  Transferred " << (((double)file_info.file_size) * 1.0E-6) << " MB in " << delta_t.count() << " sec  (" << rate_Gbps * 1000.0 << " Mbps)" << endl;
            // cout << "  I/O rate reading from file: " << delta_t_io << " sec  (" << rate_io_Gbps * 1000.0 << " Mbps)" << endl;
        }
#endif
        close(fd);
        return 0;
    }
    int postRecvWr(int id)
    {
        auto &buffer = buffers[id];
        auto buff = std::get<0>(buffer);
        auto buff_size = std::get<1>(buffer);

        struct ibv_recv_wr wr, *bad_wr;
        struct ibv_sge sge;
        bzero(&wr, sizeof(wr));
        bzero(&sge, sizeof(sge));
        wr.wr_id = id;
        wr.sg_list = &sge;
        wr.num_sge = 1;
        sge.addr = (uint64_t)buff;
        sge.length = buff_size;
        sge.lkey = mr->lkey;
        auto ret = ibv_post_recv(qp, &wr, &bad_wr);
        if (ret != 0)
        {
            cout << "ERROR: ibv_post_recv returned non zero value (" << ret << ")" << endl;
            return -1;
        }
        return 0;
    }
};

#endif