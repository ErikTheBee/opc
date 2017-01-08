from datatypes import *
import json # for string escaping

def s(string):
  return json.dumps(string, ensure_ascii=True)

def generateBooleanCode(value):
  if value:
    return "true"
  return "false"

def generateNumberCode(value):
  return str(value.value)

def generateStringCode(value):
  if not value.value:
    return "UA_STRING_NULL"
  return "UA_STRING(" + s(value.value) + ")"

def generateXmlElementCode(value):
  return "UA_XMLELEMENT(" + s(value.value) + ")"

def generateByteStringCode(value):
  return "UA_BYTESTRING(" + s(value.value) + ")"

def generateLocalizedTextCode(value):
  return "UA_LOCALIZEDTEXT(" + s(value.locale) + ", " + s(value.text) + ")"

def generateQualifiedNameCode(value):
  return "UA_QUALIFIEDNAME(ns" + str(value.ns) + ", " + s(value.name) + ")"

def generateNodeIdCode(value):
  if not value:
    return "UA_NODEID_NUMERIC(0,0)"
  if value.i != None:
    return "UA_NODEID_NUMERIC(ns%s,%s)" % (value.ns, value.i)
  elif value.s != None:
    return "UA_NODEID_STRING(ns%s,%s)" % (value.ns, s(value.s))
  raise Exception(str(value) + " no NodeID generation for bytestring and guid..")

def generateExpandedNodeIdCode(value):
    if value.i != None:
      return "UA_EXPANDEDNODEID_NUMERIC(ns%s, %s)" % (str(value.ns),str(value.i))
    elif value.s != None:
      return "UA_EXPANDEDNODEID_STRING(ns%s, %s)" % (str(value.ns), s(value.s))
    raise Exception(str(value) + " no NodeID generation for bytestring and guid..")

encoding_table = {
  Boolean: generateBooleanCode,
  Byte: generateNumberCode,
  SByte: generateNumberCode,
  Int16: generateNumberCode,
  UInt16: generateNumberCode,
  Int32: generateNumberCode,
  UInt32: generateNumberCode,
  Int64: generateNumberCode,
  UInt64: generateNumberCode,
  Float: generateNumberCode,
  Double: generateNumberCode,
  LocalizedText: generateLocalizedTextCode,
  NodeId: generateNodeIdCode,
  String: generateStringCode,
  XmlElement: generateXmlElementCode,
}

def generateValueRepresentation(value):
  vtype = type(value)
  if vtype in encoding_table:
    return encoding_table[vtype](value)
  raise Exception("Type cannot be encoded")
