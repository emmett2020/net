// #ifndef SRC_NET_REACTIVE_WAIT_OP_H_
// #define SRC_NET_REACTIVE_WAIT_OP_H_

// #include <mutex>

// #include "../unifex/all.hpp"
// #include "epoll_reactor.h"
// #include "reactor_op.h"
// #include "socket_base.h"
// #include "socket_ops.h"

// namespace net {
// using unifex::connect;
// using unifex::sender;
// using unifex::start;
// // using unifex::tag_invoke;
// using unifex::tag_t;

// template <typename Receiver>
// class ReactiveWaitOp;

// // TODO:这个类沃柑橘是多余的，将它整合进DescriptorData
// struct BaseImplementationType {
//   // The native socket representation.
//   int socket_;

//   // The current state of the socket.
//   socket_ops::SocketState state_;

//   // Per-descriptor data used by the Reactor.
//   Reactor::PerDescriptorState reactor_data_;
// };

// namespace _wait {

// class _Sender {
//  public:
//   template <template <typename...> class Variant, template <typename...> class Tuple>
//   using value_types = Variant<Tuple<>>;

//   template <template <typename...> class Variant>
//   using error_types = Variant<std::exception_ptr>;

//   static constexpr bool sends_done = true;

//   _Sender(BaseImplementationType& impl, SocketBase::WaitType w, Reactor& reactor)
//       : impl_(impl), w_(w), reactor_(reactor) {}

//   template <typename _Sender, typename Receiver>
//   friend auto tag_invoke(tag_t<unifex::connect>, _Sender&& sender, Receiver&& receiver)
//       -> ReactiveWaitOp<Receiver> {
//     std::error_code ec;
//     return ReactiveWaitOp{std::move(receiver), sender.impl_, sender.w_, sender.reactor_, ec};
//   }

//   BaseImplementationType& impl_;
//   SocketBase::WaitType w_;
//   Reactor& reactor_;
// };

// // 1. epoll相关操作和ctx强绑定。
// //
// socket来自于epoll，epoll来自于一个固定的context。给socket更换ctx
//     / scheduler即为更换epoll，新的epoll没有该socket信息，
//         //    这种操作决不允许，

//         // 1.
//         context必须提供一个单独的reactor的scheduler，涉及到epoll的操作都必须提供一个单独的scheduler,
//     //
//     scheduler的schedule函数就是返回一个sender，这个sender在connect
//         - start时，执行context.reactor.enqueue() 即可，
//           //          再次重申，scheduler返回的operation才是入队。scheduler是sender工厂
//           //         这样我们可以和on.hpp无缝关联起来, 参考manual_event_loop
//           // 2. 异步操作整体返回on的sender即可
//           // 3.
//           //
//           把context相关实现封装起来，不要对外暴露。epoll操作仅产生socket的ctx上执行，这个不许更改。
//           // 4.
//           //
//           on的第二个参数返回是我们自定的reactive相关的sender。该sender相关的operation实现了start进入epoll队列，complete时执行set_value

//           inline sender
//           auto asyncWait(BaseImplementationType&impl, SocketBase::WaitType w, Reactor&r) {
//   return _Sender{impl, w, r};
// }

// // 使用demo
// // clang-format off

// // template <typename Socket, typename MutableBuffer>
// // sender auto asyncReadSome(Socket socket, MutableBuffer buf) {
// //   return asyncWait(socket.get_impl(), SocketBase::kWaitRead, socket.get_reactor())  //
// //          | doRead(buf);
// // }

// // async_read_some(socket, buffer)
// // | then([](const MutableBuffer& buffer) { Print(buffer); })
// // | start_detached();
// // clang-format on

// };  // namespace _wait

// template <typename Receiver>
// class ReactiveWaitOp : public ReactorOp {
//  public:
//   ReactiveWaitOp(Receiver&& receiver, BaseImplementationType& impl, SocketBase::WaitType w,
//                  EpollReactor& reactor, const std::error_code& ec)
//       : ReactorOp(ec, &ReactiveWaitOp::doPerform, &ReactiveWaitOp::doComplete),
//         receiver_(std::move(receiver)),
//         impl_(impl),
//         w_(w),
//         reactor_(reactor) {}

//   // meet libunifex requirements
//   friend void tag_invoke(ex::start_t, ReactiveWaitOp& op) noexcept {
//     std::error_code ec;
//     static const bool is_continuation = false;

//     int op_type;
//     switch (op.w_) {
//       case SocketBase::kWaitRead:
//         op_type = Reactor::kReadOp;
//         break;
//       case SocketBase::kWaitWrite:
//         op_type = Reactor::kWriteOp;
//         break;
//       case SocketBase::kWaitError:
//         op_type = Reactor::kExceptOp;
//         break;
//       default:
//         op.ec_ = error::kInvalidArgument;
//         op.reactor_.postImmediateCompletion(&op, is_continuation);
//         return;
//     }
//     op.reactor_.startOp(op_type, op.impl_.socket_, op.impl_.reactor_data_, &op, is_continuation,
//                         false);
//   }

//   static Status doPerform(ReactorOp*) { return kDone; }

//   static void doComplete(void* owner, SchedulerOperation* base, const std::error_code& ec,
//                          std::size_t /*bytes_transferred*/) {
//     ::printf("do Complete \n");
//     auto& op = *static_cast<ReactiveWaitOp*>(base);

//     if (owner) {
//       ::printf("set value\n");
//       ex::set_value(std::move(op.receiver_));
//       ::printf("set value done\n");
//     }
//   }

//  private:
//   Receiver receiver_;
//   BaseImplementationType& impl_;
//   SocketBase::WaitType w_;
//   EpollReactor& reactor_;
// };

// // template <typename Operation>
// // class ReactiveReceiver {
// //  public:
// //   friend void set_value(ReactiveReceiver&& self) {
// //     ReactiveWaitOp op;
// //     start(op);
// //   }

// //  private:
// //   Operation& op_;
// // };

// }  // namespace net

// #endif  // SRC_NET_REACTIVE_WAIT_OP_H_
