/*
 * Project: curve
 * Created Date: 18-8-23
 * Author: wudemiao
 * Copyright (c) 2018 netease
 */

#ifndef CURVE_CHUNKSERVER_REQUEST_H
#define CURVE_CHUNKSERVER_REQUEST_H

#include <google/protobuf/message.h>
#include <butil/iobuf.h>
#include <brpc/controller.h>

#include <memory>

#include "proto/chunk.pb.h"
#include "include/chunkserver/chunkserver_common.h"

namespace curve {
namespace chunkserver {

using ::google::protobuf::RpcController;

class CopysetNode;
class CSDataStore;

class ChunkOpRequest : public std::enable_shared_from_this<ChunkOpRequest> {
 public:
    ChunkOpRequest();
    ChunkOpRequest(std::shared_ptr<CopysetNode> nodePtr,
                   RpcController *cntl,
                   const ChunkRequest *request,
                   ChunkResponse *response,
                   ::google::protobuf::Closure *done);

    virtual ~ChunkOpRequest() = default;

    /**
     * 处理request，实际上是Propose给相应的copyset
     */
    virtual void Process();

    /**
     * request正常情况从内存中获取上下文on apply逻辑
     * @param index:此op log entry的index
     * @param done:对应的ChunkClosure
     */
    virtual void OnApply(uint64_t index,
                         ::google::protobuf::Closure *done) = 0;

    /**
     * 从log entry反序列之后得到request详细信息进行处理，request
     * 相关的上下文和依赖的data store都是从参数传递进去的
     * 1.重启回放日志，从磁盘读取op log entry然后执行on apply逻辑
     * 2. follower执行on apply的逻辑
     * @param datastore:chunk数据持久化层
     * @param request:反序列化后得到的request 细信息
     * @param data:反序列化后得到的request要处理的数据
     */
    virtual void OnApplyFromLog(std::shared_ptr<CSDataStore> datastore,
                                const ChunkRequest &request,
                                const butil::IOBuf &data) = 0;

    /**
     * 返回request的done成员
     */
    ::google::protobuf::Closure *Closure() { return done_; }

    /**
     * 返回chunk id
     */
    ChunkID ChunkId() { return request_->chunkid(); }

    /**
     * 转发request给leader
     */
    void RedirectChunkRequest();

 public:
    /**
     * Op序列化工具函数
     * |            data                 |
     * |      op meta       |   op data  |
     * | op request length  | op request |
     * |     32 bit         |  ....      |
     * 各个字段解释如下：
     * data: encode之后的数据，实际上就是一条op log entry的data
     * op meta: 就是op的元数据，这里是op request部分的长度
     * op data: 就是request通过protobuf序列化后的数据
     * @param cntl:rpc控制器
     * @param request:Chunk Request
     * @param data:出参，存放序列化好的数据，用户自己保证data!=nullptr
     * @return 0成功，-1失败
     */
    static int Encode(brpc::Controller *cntl,
                      const ChunkRequest *request,
                      butil::IOBuf *data);

    /**
     * 反序列化，从log entry得到ChunkOpRequest，当前反序列出的ChunkRequest和data
     * 都会从出参传出去，而不会放在ChunkOpRequest的成员变量里面
     * @param log:op log entry
     * @param request: 出参，存放反序列上下文
     * @param data:出参，op操作的数据
     * @return nullptr,失败，否则返回相应的ChunkOpRequest
     */
    static std::shared_ptr<ChunkOpRequest> Decode(butil::IOBuf log,
                                                  ChunkRequest *request,
                                                  butil::IOBuf *data);

 protected:
    /**
     * 打包request为braft::task，propose给相应的复制组
     * @return 0成功，-1失败
     */
    int Propose();

 protected:
    // chunk持久化接口
    std::shared_ptr<CSDataStore> datastore_;
    // 复制组
    std::shared_ptr<CopysetNode> node_;
    // rpc controller
    brpc::Controller *cntl_;
    // rpc 请求
    const ChunkRequest *request_;
    // rpc 返回
    ChunkResponse *response_;
    // rpc done closure
    ::google::protobuf::Closure *done_;
};

class DeleteChunkRequest : public ChunkOpRequest {
 public:
    DeleteChunkRequest() :
        ChunkOpRequest() {}
    DeleteChunkRequest(std::shared_ptr<CopysetNode> nodePtr,
                       RpcController *cntl,
                       const ChunkRequest *request,
                       ChunkResponse *response,
                       ::google::protobuf::Closure *done) :
        ChunkOpRequest(nodePtr,
                       cntl,
                       request,
                       response,
                       done) {}
    virtual ~DeleteChunkRequest() = default;

    void OnApply(uint64_t index, ::google::protobuf::Closure *done) override;
    void OnApplyFromLog(std::shared_ptr<CSDataStore> datastore,
                        const ChunkRequest &request,
                        const butil::IOBuf &data) override;
};

class ReadChunkRequest : public ChunkOpRequest {
 public:
    ReadChunkRequest() :
        ChunkOpRequest() {}
    ReadChunkRequest(std::shared_ptr<CopysetNode> nodePtr,
                     RpcController *cntl,
                     const ChunkRequest *request,
                     ChunkResponse *response,
                     ::google::protobuf::Closure *done) :
        ChunkOpRequest(nodePtr,
                       cntl,
                       request,
                       response,
                       done) {}
    virtual ~ReadChunkRequest() = default;

    void Process() override;
    void OnApply(uint64_t index, ::google::protobuf::Closure *done) override;
    void OnApplyFromLog(std::shared_ptr<CSDataStore> datastore,
                        const ChunkRequest &request,
                        const butil::IOBuf &data) override;
};

class WriteChunkRequest : public ChunkOpRequest {
 public:
    WriteChunkRequest() :
        ChunkOpRequest() {}
    WriteChunkRequest(std::shared_ptr<CopysetNode> nodePtr,
                      RpcController *cntl,
                      const ChunkRequest *request,
                      ChunkResponse *response,
                      ::google::protobuf::Closure *done) :
        ChunkOpRequest(nodePtr,
                       cntl,
                       request,
                       response,
                       done) {}
    virtual ~WriteChunkRequest() = default;

    void OnApply(uint64_t index, ::google::protobuf::Closure *done);
    void OnApplyFromLog(std::shared_ptr<CSDataStore> datastore,
                        const ChunkRequest &request,
                        const butil::IOBuf &data) override;
};

class ReadSnapshotRequest : public ChunkOpRequest {
 public:
    ReadSnapshotRequest() :
        ChunkOpRequest() {}
    ReadSnapshotRequest(std::shared_ptr<CopysetNode> nodePtr,
                        RpcController *cntl,
                        const ChunkRequest *request,
                        ChunkResponse *response,
                        ::google::protobuf::Closure *done) :
        ChunkOpRequest(nodePtr,
                       cntl,
                       request,
                       response,
                       done) {}
    virtual ~ReadSnapshotRequest() = default;

    void OnApply(uint64_t index, ::google::protobuf::Closure *done) override;
    void OnApplyFromLog(std::shared_ptr<CSDataStore> datastore,
                        const ChunkRequest &request,
                        const butil::IOBuf &data) override;
};

class DeleteSnapshotRequest : public ChunkOpRequest {
 public:
    DeleteSnapshotRequest() :
        ChunkOpRequest() {}
    DeleteSnapshotRequest(std::shared_ptr<CopysetNode> nodePtr,
                          RpcController *cntl,
                          const ChunkRequest *request,
                          ChunkResponse *response,
                          ::google::protobuf::Closure *done) :
        ChunkOpRequest(nodePtr,
                       cntl,
                       request,
                       response,
                       done) {}
    virtual ~DeleteSnapshotRequest() = default;

    void OnApply(uint64_t index, ::google::protobuf::Closure *done) override;
    void OnApplyFromLog(std::shared_ptr<CSDataStore> datastore,
                        const ChunkRequest &request,
                        const butil::IOBuf &data) override;
};

}  // namespace chunkserver
}  // namespace curve

#endif  // CURVE_CHUNKSERVER_REQUEST_H
