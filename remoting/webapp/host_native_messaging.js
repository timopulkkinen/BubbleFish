// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Class to communicate with the Host components via Native Messaging.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @constructor
 * @extends {remoting.HostPlugin}
 */
remoting.HostNativeMessaging = function() {
  /**
   * @type {number}
   * @private
   */
  this.nextId_ = 0;

  /**
   * @type {Object.<number, {callback:function(...):void, type:string}>}
   * @private
   */
  this.pendingReplies_ = {};

  /** @private */
  this.port_ = chrome.runtime.connectNative(
      'chrome-remote-desktop-native-host');
  this.port_.onMessage.addListener(this.onIncomingMessage_.bind(this));
};

/**
 * @return {boolean} True if native messaging is supported.
 */
remoting.HostNativeMessaging.isSupported = function() {
  // TODO(lambroslambrou): This needs to perform an asynchronous "hello"
  // exchange with the native host component (possibly getting its version
  // number at the same time), and report the result via a regular callback or
  // an error callback. For now, simply return false here.
  return false;
};

/**
 * Verifies that |object| is of type |type|, logging an error if not.
 *
 * @param {string} name Name of the object, to be included in the error log.
 * @param {*} object Object to test.
 * @param {string} type Expected type of the object.
 * @return {boolean} Result of test.
 */
function checkType_(name, object, type) {
  if (typeof(object) !== type) {
    console.error('NativeMessaging: "', name, '" expected to be of type "',
                  type, '", got: ', object);
    return false;
  }
  return true;
}

/**
 * Returns |result| as an AsyncResult. If |result| is not valid, returns null
 * and logs an error.
 *
 * @param {*} result
 * @return {remoting.HostController.AsyncResult?} Converted result.
 */
function asAsyncResult_(result) {
  if (!checkType_('result', result, 'number')) {
    return null;
  }
  for (var i in remoting.HostController.AsyncResult) {
    if (remoting.HostController.AsyncResult[i] == result) {
      return remoting.HostController.AsyncResult[i];
    }
  }
  console.error('NativeMessaging: unexpected result code: ', result);
  return null;
}

/**
 * Attaches a new ID to the supplied message, and posts it to the Native
 * Messaging port, adding |callback| to the list of pending replies.
 * |message| should have its 'type' field set, and any other fields set
 * depending on the message type.
 *
 * @param {{type: string}} message The message to post.
 * @param {function(...):void} callback The callback to be triggered on
 *     response.
 * @return {void} Nothing.
 * @private
 */
remoting.HostNativeMessaging.prototype.postMessage_ = function(message,
                                                               callback) {
  var id = this.nextId_++;
  message['id'] = id;
  this.pendingReplies_[id] = {
    callback: callback,
    type: message.type + 'Response'
  }
  this.port_.postMessage(message);
};

/**
 * Handler for incoming Native Messages.
 *
 * @param {Object} message The received message.
 * @return {void} Nothing.
 * @private
 */
remoting.HostNativeMessaging.prototype.onIncomingMessage_ = function(message) {
  /** @type {number} */
  var id = message['id'];
  if (typeof(id) != 'number') {
    console.error('NativeMessaging: missing or non-numeric id');
    return;
  }
  var reply = this.pendingReplies_[id];
  if (!reply) {
    console.error('NativeMessaging: unexpected id: ', id);
    return;
  }
  delete this.pendingReplies_[id];

  /** @type {string} */
  var type = message['type'];
  if (!checkType_('type', type, 'string')) {
    return;
  }
  if (type != reply.type) {
    console.error('NativeMessaging: expected reply type: ', reply.type,
                  ', got: ', type);
    return;
  }

  var callback = reply.callback;

  // TODO(lambroslambrou): Errors here should be passed to an error-callback
  // supplied by the caller of this interface.
  switch (type) {
    case 'getHostNameResponse':
      /** @type {*} */
      var hostname = message['hostname'];
      if (checkType_('hostname', hostname, 'string')) {
        callback(hostname);
      }
      break;

    case 'getPinHashResponse':
      /** @type {*} */
      var hash = message['hash'];
      if (checkType_('hash', hash, 'string')) {
        callback(hash);
      }
      break;

    case 'generateKeyPairResponse':
      /** @type {*} */
      var private_key = message['private_key'];
      /** @type {*} */
      var public_key = message['public_key'];
      if (checkType_('private_key', private_key, 'string') &&
          checkType_('public_key', public_key, 'string')) {
        callback(private_key, public_key);
      }
      break;

    case 'updateDaemonConfigResponse':
      var result = asAsyncResult_(message['result']);
      if (result != null) {
        callback(result);
      }
      break;

    case 'getDaemonConfigResponse':
      /** @type {*} */
      var config = message['config'];
      if (checkType_('config', config, 'string')) {
        callback(config);
      }
      break;

    case 'getDaemonVersionResponse':
      /** @type {*} */
      var version = message['version'];
      if (checkType_('version', version, 'string')) {
        callback(version);
      }
      break;

    case 'getUsageStatsConsentResponse':
      /** @type {*} */
      var supported = message['supported'];
      /** @type {*} */
      var allowed = message['allowed'];
      /** @type {*} */
      var set_by_policy = message['set_by_policy'];
      if (checkType_('supported', supported, 'boolean') &&
          checkType_('allowed', allowed, 'boolean') &&
          checkType_('set_by_policy', set_by_policy, 'boolean')) {
        callback(supported, allowed, set_by_policy);
      }
      break;

    case 'startDaemonResponse':
    case 'stopDaemonResponse':
      var result = asAsyncResult_(message['result']);
      if (result != null) {
        callback(result);
      }
      break;

    default:
      console.error('Unexpected native message: ', message);
  }
};

/**
 * @param {string} email The email address of the connector.
 * @param {string} token The access token for the connector.
 * @return {void} Nothing.
 */
remoting.HostNativeMessaging.prototype.connect = function(email, token) {
  console.error('NativeMessaging: connect() not implemented.');
};

/** @return {void} Nothing. */
remoting.HostNativeMessaging.prototype.disconnect = function() {
  console.error('NativeMessaging: disconnect() not implemented.');
};

/**
 * @param {function(string):string} callback Pointer to chrome.i18n.getMessage.
 * @return {void} Nothing.
 */
remoting.HostNativeMessaging.prototype.localize = function(callback) {
  console.error('NativeMessaging: localize() not implemented.');
};

/**
 * @param {function(string):void} callback Callback to be called with the
 *     local hostname.
 * @return {void} Nothing.
 */
remoting.HostNativeMessaging.prototype.getHostName = function(callback) {
  this.postMessage_({type: 'getHostName'}, callback);
};

/**
 * Calculates PIN hash value to be stored in the config, passing the resulting
 * hash value base64-encoded to the callback.
 *
 * @param {string} hostId The host ID.
 * @param {string} pin The PIN.
 * @param {function(string):void} callback Callback.
 * @return {void} Nothing.
 */
remoting.HostNativeMessaging.prototype.getPinHash = function(hostId, pin,
                                                             callback) {
  this.postMessage_({
      type: 'getPinHash',
      hostId: hostId,
      pin: pin
  }, callback);
};

/**
 * Generates new key pair to use for the host. The specified callback is called
 * when the key is generated. The key is returned in format understood by the
 * host (PublicKeyInfo structure encoded with ASN.1 DER, and then BASE64).
 *
 * @param {function(string, string):void} callback Callback.
 * @return {void} Nothing.
 */
remoting.HostNativeMessaging.prototype.generateKeyPair = function(callback) {
  this.postMessage_({type: 'generateKeyPair'}, callback);
};

/**
 * Updates host config with the values specified in |config|. All
 * fields that are not specified in |config| remain
 * unchanged. Following parameters cannot be changed using this
 * function: host_id, xmpp_login. Error is returned if |config|
 * includes these parameters. Changes take effect before the callback
 * is called.
 *
 * @param {string} config The new config parameters, JSON encoded dictionary.
 * @param {function(remoting.HostController.AsyncResult):void} callback
 *     Callback to be called when finished.
 * @return {void} Nothing.
 */
remoting.HostNativeMessaging.prototype.updateDaemonConfig =
    function(config, callback) {
  this.postMessage_({
      type: 'updateDaemonConfig',
      config: config
  }, callback);
};

/**
 * Loads daemon config. The config is passed as a JSON formatted string to the
 * callback.
 *
 * @param {function(string):void} callback Callback.
 * @return {void} Nothing.
 */
remoting.HostNativeMessaging.prototype.getDaemonConfig = function(callback) {
  this.postMessage_({type: 'getDaemonConfig'}, callback);
};

/**
 * Retrieves daemon version. The version is passed to the callback as a dotted
 * decimal string of the form major.minor.build.patch.
 *
 * @param {function(string):void} callback Callback.
 * @return {void} Nothing.
 */
remoting.HostNativeMessaging.prototype.getDaemonVersion = function(callback) {
  this.postMessage_({type: 'getDaemonVersion'}, callback);
};

/**
 * Get the user's consent to crash reporting. The consent flags are passed to
 * the callback as booleans: supported, allowed, set-by-policy.
 *
 * @param {function(boolean, boolean, boolean):void} callback Callback.
 * @return {void} Nothing.
 */
remoting.HostNativeMessaging.prototype.getUsageStatsConsent =
    function(callback) {
  this.postMessage_({type: 'getUsageStatsConsent'}, callback);
};

/**
 * Starts the daemon process with the specified configuration.
 *
 * @param {string} config Host configuration.
 * @param {boolean} consent Consent to report crash dumps.
 * @param {function(remoting.HostController.AsyncResult):void} callback
 *     Callback.
 * @return {void} Nothing.
 */
remoting.HostNativeMessaging.prototype.startDaemon = function(
    config, consent, callback) {
  this.postMessage_({
      type: 'startDaemon',
      config: config,
      consent: consent
  }, callback);
};

/**
 * Stops the daemon process.
 *
 * @param {function(remoting.HostController.AsyncResult):void} callback
 *     Callback.
 * @return {void} Nothing.
 */
remoting.HostNativeMessaging.prototype.stopDaemon = function(callback) {
  this.postMessage_({type: 'stopDaemon'}, callback);
};
