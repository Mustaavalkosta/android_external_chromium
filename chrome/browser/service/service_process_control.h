// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICE_SERVICE_PROCESS_CONTROL_H_
#define CHROME_BROWSER_SERVICE_SERVICE_PROCESS_CONTROL_H_

#include <queue>
#include <string>

#include "base/id_map.h"
#include "base/callback.h"
#include "base/process.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "chrome/common/service_process_type.h"
#include "ipc/ipc_sync_channel.h"

class Profile;

// A ServiceProcessControl works as a portal between the service process and
// the browser process.
//
// It is used to start and terminate the service process. It is also used
// to send and receive IPC messages from the service process.
//
// THREADING
//
// This class is accessed on the UI thread through some UI actions. It then
// talks to the IPC channel on the IO thread.
class ServiceProcessControl : public IPC::Channel::Sender,
                              public IPC::Channel::Listener {
 public:
  typedef IDMap<ServiceProcessControl>::iterator iterator;
  typedef std::queue<IPC::Message> MessageQueue;

  // An interface for handling messages received from the service process.
  class MessageHandler {
   public:
    virtual ~MessageHandler() {}
    // This is a test signal sent from the service process. This can be used
    // the healthiness of the service.
    virtual void OnGoodDay() = 0;
  };

  // Construct a ServiceProcessControl with |profile| and a specific |type|.
  ServiceProcessControl(Profile* profile, ServiceProcessType type);
  virtual ~ServiceProcessControl();

  // Return the user profile associated with this service process.
  Profile* profile() const { return profile_; }

  // Return the type of this object.
  ServiceProcessType type() const { return type_; }

  // Return true if this object is connected to the service.
  bool is_connected() const { return channel_.get() != NULL; }

  // Create a new service process and connects to it.
  // |launch_done_task| is called if launching the service process has failed
  // or we have successfully launched the process and connected to it.
  // If the service process is already running this method will try to connect
  // to the service process.
  void Launch(Task* launch_done_task);

  // IPC::Channel::Listener implementation.
  virtual void OnMessageReceived(const IPC::Message& message);
  virtual void OnChannelConnected(int32 peer_pid);
  virtual void OnChannelError();

  // IPC::Channel::Sender implementation
  virtual bool Send(IPC::Message* message);

  // Message handlers
  void OnGoodDay();
  void OnCloudPrintProxyIsEnabled(bool enabled, std::string email);

  // Send a hello message to the service process for testing purpose.
  // Return true if the message was sent.
  bool SendHello();

  // Send a shutdown message to the service process. IPC channel will be
  // destroyed after calling this method.
  // Return true if the message was sent.
  bool Shutdown();

  // Send a message to enable the remoting service in the service process.
  // Return true if the message was sent.
  bool EnableRemotingWithTokens(const std::string& user,
                                const std::string& remoting_token,
                                const std::string& talk_token);

  // Send a message to the service process to request a response
  // containing the enablement status of the cloud print proxy and the
  // registered email address.  The callback gets the information when
  // received.
  bool GetCloudPrintProxyStatus(
      Callback2<bool, std::string>::Type* cloud_print_status_callback);

  // Set the message handler for receiving messages from the service process.
  // TODO(hclam): Allow more than 1 handler.
  void SetMessageHandler(MessageHandler* message_handler) {
    message_handler_ = message_handler;
  }

 private:
  class Launcher;

  // Method called by Launcher when the service process is launched.
  void OnProcessLaunched(Task* launch_done_task);

  // Used internally to connect to the service process. |task| is executed
  // when the connection is made or an error occurred.
  void ConnectInternal(Task* task);

  Profile* profile_;
  ServiceProcessType type_;

  // IPC channel to the service process.
  scoped_ptr<IPC::SyncChannel> channel_;

  // Service process launcher.
  scoped_refptr<Launcher> launcher_;

  // Callback that gets invoked when the channel is connected or failed to
  // connect.
  scoped_ptr<Task> connect_done_task_;

  // Callback that gets invoked when a status message is received from
  // the cloud print proxy.
  scoped_ptr<Callback2<bool, std::string>::Type> cloud_print_status_callback_;

  // Handler for messages from service process.
  MessageHandler* message_handler_;
};

#endif  // CHROME_BROWSER_SERVICE_SERVICE_PROCESS_CONTROL_H_