// GENERATED CODE -- DO NOT EDIT!

'use strict';
var grpc = require('@grpc/grpc-js');
var peerbridge_pb = require('./peerbridge_pb.js');

function serialize_peerbridge_StunInfoRequest(arg) {
  if (!(arg instanceof peerbridge_pb.StunInfoRequest)) {
    throw new Error('Expected argument of type peerbridge.StunInfoRequest');
  }
  return Buffer.from(arg.serializeBinary());
}

function deserialize_peerbridge_StunInfoRequest(buffer_arg) {
  return peerbridge_pb.StunInfoRequest.deserializeBinary(new Uint8Array(buffer_arg));
}

function serialize_peerbridge_StunInfoResponse(arg) {
  if (!(arg instanceof peerbridge_pb.StunInfoResponse)) {
    throw new Error('Expected argument of type peerbridge.StunInfoResponse');
  }
  return Buffer.from(arg.serializeBinary());
}

function deserialize_peerbridge_StunInfoResponse(buffer_arg) {
  return peerbridge_pb.StunInfoResponse.deserializeBinary(new Uint8Array(buffer_arg));
}


// Service definition for PeerBridge
var PeerBridgeServiceService = exports.PeerBridgeServiceService = {
  // RPC to get STUN information (public IP and port)
getStunInfo: {
    path: '/peerbridge.PeerBridgeService/GetStunInfo',
    requestStream: false,
    responseStream: false,
    requestType: peerbridge_pb.StunInfoRequest,
    responseType: peerbridge_pb.StunInfoResponse,
    requestSerialize: serialize_peerbridge_StunInfoRequest,
    requestDeserialize: deserialize_peerbridge_StunInfoRequest,
    responseSerialize: serialize_peerbridge_StunInfoResponse,
    responseDeserialize: deserialize_peerbridge_StunInfoResponse,
  },
  // We will define other RPC methods here later
};

exports.PeerBridgeServiceClient = grpc.makeGenericClientConstructor(PeerBridgeServiceService, 'PeerBridgeService');
