# -*- coding: utf-8 -*-
# Generated by the protocol buffer compiler.  DO NOT EDIT!
# source: message.proto

from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from google.protobuf import reflection as _reflection
from google.protobuf import symbol_database as _symbol_database
# @@protoc_insertion_point(imports)

_sym_db = _symbol_database.Default()




DESCRIPTOR = _descriptor.FileDescriptor(
  name='message.proto',
  package='',
  syntax='proto3',
  serialized_options=None,
  create_key=_descriptor._internal_create_key,
  serialized_pb=b'\n\rmessage.proto\"%\n\x08HelloMsg\x12\r\n\x05greet\x18\x01 \x01(\t\x12\n\n\x02id\x18\x02 \x01(\x05\x62\x06proto3'
)




_HELLOMSG = _descriptor.Descriptor(
  name='HelloMsg',
  full_name='HelloMsg',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  create_key=_descriptor._internal_create_key,
  fields=[
    _descriptor.FieldDescriptor(
      name='greet', full_name='HelloMsg.greet', index=0,
      number=1, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=b"".decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR,  create_key=_descriptor._internal_create_key),
    _descriptor.FieldDescriptor(
      name='id', full_name='HelloMsg.id', index=1,
      number=2, type=5, cpp_type=1, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR,  create_key=_descriptor._internal_create_key),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  serialized_options=None,
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=17,
  serialized_end=54,
)

DESCRIPTOR.message_types_by_name['HelloMsg'] = _HELLOMSG
_sym_db.RegisterFileDescriptor(DESCRIPTOR)

HelloMsg = _reflection.GeneratedProtocolMessageType('HelloMsg', (_message.Message,), {
  'DESCRIPTOR' : _HELLOMSG,
  '__module__' : 'message_pb2'
  # @@protoc_insertion_point(class_scope:HelloMsg)
  })
_sym_db.RegisterMessage(HelloMsg)


# @@protoc_insertion_point(module_scope)
