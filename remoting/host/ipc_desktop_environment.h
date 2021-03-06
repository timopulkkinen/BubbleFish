// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IPC_DESKTOP_ENVIRONMENT_H_
#define REMOTING_HOST_IPC_DESKTOP_ENVIRONMENT_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "remoting/host/curtain_mode.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/desktop_session_connector.h"

namespace base {
class SingleThreadTaskRunner;
}  // base

namespace IPC {
class Sender;
}  // namespace IPC

namespace remoting {

class DesktopSessionProxy;
class ScreenResolution;

// A variant of desktop environment integrating with the desktop by means of
// a helper process and talking to that process via IPC.
class IpcDesktopEnvironment : public DesktopEnvironment {
 public:
  // |desktop_session_connector| is used to bind DesktopSessionProxy to
  // a desktop session, to be notified every time the desktop process is
  // restarted.
  IpcDesktopEnvironment(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      const std::string& client_jid,
      const base::Closure& disconnect_callback,
      base::WeakPtr<DesktopSessionConnector> desktop_session_connector,
      bool virtual_terminal);
  virtual ~IpcDesktopEnvironment();

  // DesktopEnvironment implementation.
  virtual scoped_ptr<AudioCapturer> CreateAudioCapturer(
      scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner) OVERRIDE;
  virtual scoped_ptr<InputInjector> CreateInputInjector(
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) OVERRIDE;
  virtual scoped_ptr<SessionController> CreateSessionController() OVERRIDE;
  virtual scoped_ptr<media::ScreenCapturer> CreateVideoCapturer(
      scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner) OVERRIDE;

 private:
  // Task runner on which public methods of this class should be called.
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

  scoped_refptr<DesktopSessionProxy> desktop_session_proxy_;

  DISALLOW_COPY_AND_ASSIGN(IpcDesktopEnvironment);
};

// Used to create IpcDesktopEnvironment objects integrating with the desktop via
// a helper process and talking to that process via IPC.
class IpcDesktopEnvironmentFactory
    : public CurtainMode,
      public DesktopEnvironmentFactory,
      public DesktopSessionConnector {
 public:
  // Passes a reference to the IPC channel connected to the daemon process and
  // relevant task runners. |daemon_channel| must outlive this object.
  IpcDesktopEnvironmentFactory(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      IPC::Sender* daemon_channel);
  virtual ~IpcDesktopEnvironmentFactory();

  // CurtainMode implementation.
  virtual void SetActivated(bool activated) OVERRIDE;

  // DesktopEnvironmentFactory implementation.
  virtual scoped_ptr<DesktopEnvironment> Create(
      const std::string& client_jid,
      const base::Closure& disconnect_callback) OVERRIDE;
  virtual bool SupportsAudioCapture() const OVERRIDE;

  // DesktopSessionConnector implementation.
  virtual void ConnectTerminal(
      DesktopSessionProxy* desktop_session_proxy,
      const ScreenResolution& resolution,
      bool virtual_terminal) OVERRIDE;
  virtual void DisconnectTerminal(
      DesktopSessionProxy* desktop_session_proxy) OVERRIDE;
  virtual void SetScreenResolution(
      DesktopSessionProxy* desktop_session_proxy,
      const ScreenResolution& resolution) OVERRIDE;
  virtual void OnDesktopSessionAgentAttached(
      int terminal_id,
      base::ProcessHandle desktop_process,
      IPC::PlatformFileForTransit desktop_pipe) OVERRIDE;
  virtual void OnTerminalDisconnected(int terminal_id) OVERRIDE;

 private:
  // Task runner on which public methods of this class should be called.
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

  // Task runner used for running background I/O.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // True if curtain mode is activated.
  bool curtain_activated_;

  // IPC channel connected to the daemon process.
  IPC::Sender* daemon_channel_;

  // List of DesktopEnvironment instances we've told the daemon process about.
  typedef std::map<int, DesktopSessionProxy*> ActiveConnectionsList;
  ActiveConnectionsList active_connections_;

  // Factory for weak pointers to DesktopSessionConnector interface.
  base::WeakPtrFactory<DesktopSessionConnector> connector_factory_;

  // Next desktop session ID. IDs are allocated sequentially starting from 0.
  // This gives us more than 67 years of unique IDs assuming a new ID is
  // allocated every second.
  int next_id_;

  DISALLOW_COPY_AND_ASSIGN(IpcDesktopEnvironmentFactory);
};

}  // namespace remoting

#endif  // REMOTING_HOST_IPC_DESKTOP_ENVIRONMENT_H_
