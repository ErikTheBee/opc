#!/usr/bin/env/python
# -*- coding: utf-8 -*-

###
### Author:  Chris Iatrou (ichrispa@core-vector.net)
### Version: rev 13
###
### This program was created for educational purposes and has been
### contributed to the open62541 project by the author. All licensing
### terms for this source is inherited by the terms and conditions
### specified for by the open62541 project (see the projects readme
### file for more information on the LGPL terms and restrictions).
###
### This program is not meant to be used in a production environment. The
### author is not liable for any complications arising due to the use of
### this program.
###

import sys
from time import strftime, strptime
import logging; logger = logging.getLogger(__name__)
import xml.dom.minidom as dom

from constants import *

if sys.version_info[0] >= 3:
  # strings are already parsed to unicode
  def unicode(s):
    return s

def parseXMLValue(xml, klassname):
  klass = getattr(sys.modules[__name__], klassname)
  if not klass:
    return None
  return klass(xml)

def parseValue(xmlvalue):
    if xmlvalue == None or xmlvalue.nodeType != xmlvalue.ELEMENT_NODE:
      logger.error("Expected XML Element, but got junk...")
      return

    if not "value" in xmlvalue.tagName.lower():
      logger.error("Expected <Value> , but found " + xmlvalue.tagName + \
                   " instead. Value will not be parsed.")
      return

    if len(xmlvalue.childNodes) == 0:
      logger.error("Expected childnodes for value, but none were found...")
      return

    for n in xmlvalue.childNodes:
      if n.nodeType == n.ELEMENT_NODE:
        xmlvalue = n
        break

    if "ListOf" in xmlvalue.tagName:
      value = []
      klassname = xmlvalue.tagName[6:]
      for el in xmlvalue.childNodes:
        if not el.nodeType == el.ELEMENT_NODE:
          continue
        value.append(parseXMLValue(el, klassname))
      return value
    return parseXMLValue(xmlvalue, xmlvalue.tagName)

#################
# Builtin Types #
#################

class Value(object):
  def __str__(self):
    if not self.value:
      return str(type(self)) + "()"
    return str(type(self)) + "(%s)" % self.value

  def __eq__(v1, v2):
    return str(v1) == str(v2)

  def __repr__(self):
    return str(self)

class Boolean(Value):
  def __init__(self, xmlelement = None):
    self.value = "false"
    if xmlelement:
      self.parseXML(xmlelement)

  def parseXML(self, xmlvalue):
    if xmlvalue.firstChild and xmlvalue.firstChild.data:
      if "false" in unicode(xmlvalue.firstChild.data).lower():
        self.value = "false"
      else:
        self.value = "true"

class Integer(Value):
  def __init__(self, xmlelement = None):
    self.value = 0
    if xmlelement:
      self.parseXML(xmlelement)

  def parseXML(self, xmlvalue):
    if xmlvalue.firstChild and xmlvalue.firstChild.data:
      self.value = int(unicode(xmlvalue.firstChild.data))

class Byte(Integer):
  pass

class SByte(Integer):
  pass

class Int16(Integer):
  pass

class UInt16(Integer):
  pass

class Int32(Integer):
  pass

class UInt32(Integer):
  pass

class Int64(Integer):
  pass

class UInt64(Value):
  pass

class Float(Value):
  def __init__(self, xmlelement = None):
    self.value = 0.0
    if xmlelement:
      self.parseXML(xmlelement)

  def parseXML(self, xmlvalue):
    if xmlvalue.firstChild and xmlvalue.firstChild.data:
      self.value = float(unicode(xmlvalue.firstChild.data))

class Double(Value):
  def __init__(self, xmlelement = None):
    self.value = 0.0
    if xmlelement:
      self.parseXML(xmlelement)

  def parseXML(self, xmlvalue):
    if xmlvalue.firstChild and xmlvalue.firstChild.data:
      self.value = float(unicode(xmlvalue.firstChild.data))

class String(Value):
  def __init__(self, xmlelement = None):
    self.value = None
    if xmlelement:
      self.parseXML(xmlelement)

  def parseXML(self, xmlvalue):
    if xmlvalue.firstChild:
      self.value = unicode(xmlvalue.firstChild.data)

class XmlElement(String):
  pass

class ByteString(String):
  pass

class ExtensionObject(Value):
  def __init__(self, xmlelement = None):
    self.value = None
    if xmlelement:
      self.parseXML(xmlelement)

  def parseXML(self, xmlelement):
    pass

class LocalizedText(Value):
  def __init__(self, xmlvalue = None):
    self.locale = 'en_US'
    self.text = ''
    if xmlvalue:
      self.parseXML(xmlvalue)

  def parseXML(self, xmlvalue):
    # Expect <LocalizedText> or <AliasName>
    #          <Locale>xx_XX</Locale>
    #          <Text>TextText</Text>
    #        <LocalizedText> or </AliasName>
    if not isinstance(xmlvalue, dom.Element):
      self.text = xmlvalue
      return
    tmp = xmlvalue.getElementsByTagName("Locale")
    if len(tmp) > 0 and tmp[0].firstChild != None:
        self.locale = unicode(tmp[0].firstChild.data)
    tmp = xmlvalue.getElementsByTagName("Text")
    if len(tmp) > 0 and tmp[0].firstChild != None:
        self.text = unicode(tmp[0].firstChild.data)

  def __str__(self):
    return "LocalizedText(%s,%s)" % (self.locale, self.text)

class NodeId(Value):
  def __init__(self, idstring = None):
    self.i = None
    self.b = None
    self.g = None
    self.s = None
    self.ns = 0

    if not idstring:
      self.i = 0
      return

    # The ID will encoding itself appropriatly as string. If multiple ID's
    # (numeric, string, guid) are defined, the order of preference for the ID
    # string is always numeric, guid, bytestring, string. Binary encoding only
    # applies to numeric values (UInt16).
    idparts = idstring.strip().split(";")
    for p in idparts:
      if p[:2] == "ns":
        self.ns = int(p[3:])
      elif p[:2] == "i=":
        self.i = int(p[2:])
      elif p[:2] == "o=":
        self.b = p[2:]
      elif p[:2] == "g=":
        tmp = []
        self.g = p[2:].split("-")
        for i in self.g:
          i = "0x"+i
          tmp.append(int(i,16))
        self.g = tmp
      elif p[:2] == "s=":
        self.s = p[2:]
      else:
        raise Exception("Not a valid NodeId")

  def __str__(self):
    s = "ns="+str(self.ns)+";"
    # Order of preference is numeric, guid, bytestring, string
    if self.i != None:
      return s + "i="+str(self.i)
    elif self.g != None:
      s = s + "g="
      tmp = []
      for i in self.g:
        tmp.append(hex(i).replace("0x",""))
      for i in tmp:
        s = s + "-" + i
      return s.replace("g=-","g=")
    elif self.b != None:
      return s + "b="+str(self.b)
    elif self.s != None:
      return s + "s="+str(self.s)
    raise Exception("Not a valid NodeId")

  def __repr__(self):
    return str(self)

  def __hash__(self):
    return hash(str(self))

class ExpandedNodeId(Value):
  def __init__(self, xmlelement = None):
    if xmlelement:
      self.parseXML(xmlelement)

  def parseXML(self, xmlvalue):
    logger.debug("Not implemented", LOG_LEVEL_ERR)

class DateTime(Value):
  def __init__(self, xmlelement = None):
    if xmlelement:
      self.parseXML(xmlelement)

  def parseXML(self, xmlvalue):
    # Expect <DateTime> or <AliasName>
    #        2013-08-13T21:00:05.0000L
    #        </DateTime> or </AliasName>
    self.checkXML(xmlvalue)
    if xmlvalue.firstChild == None :
      # Catch XML <DateTime /> by setting the value to a default
      self.value = strptime(strftime("%Y-%m-%dT%H:%M%S"), "%Y-%m-%dT%H:%M%S")
    else:
      timestr = unicode(xmlvalue.firstChild.data)
      # .NET tends to create this garbage %Y-%m-%dT%H:%M:%S.0000z
      # strip everything after the "." away for a posix time_struct
      if "." in timestr:
        timestr = timestr[:timestr.index(".")]
      # If the last character is not numeric, remove it
      while len(timestr)>0 and not timestr[-1] in "0123456789":
        timestr = timestr[:-1]
      try:
        self.value = strptime(timestr, "%Y-%m-%dT%H:%M:%S")
      except:
        logger.error("Timestring format is illegible. Expected 2001-01-30T21:22:23, but got " + \
                     timestr + " instead. Time will be defaultet to now()")
        self.value = strptime(strftime("%Y-%m-%dT%H:%M%S"), "%Y-%m-%dT%H:%M%S")

class QualifiedName(Value):
  def __init__(self, xmlelement = None):
    self.numericRepresentation = BUILTINTYPE_TYPEID_QUALIFIEDNAME
    self.ns = 0
    self.name = ''
    if xmlelement:
      self.parseXML(xmlelement)

  def parseXML(self, xmlvalue):
    # Expect <QualifiedName> or <AliasName>
    #           <NamespaceIndex>Int16<NamespaceIndex>
    #           <Name>SomeString<Name>
    #        </QualifiedName> or </AliasName>
    if not isinstance(xmlvalue, dom.Element):
      colonindex = xmlvalue.find(":")
      if colonindex == -1 or not xmlvalue[:colonindex].isdigit():
        self.name = xmlvalue
      else:
        self.name = xmlvalue[colonindex+1:]
        self.ns = int(xmlvalue[:colonindex])
      return

    # Is a namespace index passed?
    if len(xmlvalue.getElementsByTagName("NamespaceIndex")) != 0:
      self.ns = int(xmlvalue.getElementsByTagName("NamespaceIndex")[0].firstChild.data)
    if len(xmlvalue.getElementsByTagName("Name")) != 0:
      self.name = xmlvalue.getElementsByTagName("Name")[0].firstChild.data

  def __str__(self):
    return "QualifiedName(%s,%s)" % (str(self.ns), self.name)

class StatusCode(UInt32):
  pass

class DiagnosticInfo(Value):
  def parseXML(self, xmlvalue):
    logger.warn("Not implemented")

class Guid(Value):
  def __init__(self, xmlelement = None):
    self.value = [0,0,0,0] # Catch XML <Guid /> by setting the value to a default
    if xmlelement:
      self.parseXML(xmlelement)

  def parseXML(self, xmlvalue):
    self.checkXML(xmlvalue)
    if xmlvalue.firstChild:
      self.value = unicode(xmlvalue.firstChild.data)
      self.value = self.value.replace("{","")
      self.value = self.value.replace("}","")
      self.value = self.value.split("-")
      tmp = []
      for g in self.value:
        try:
          tmp.append(int("0x"+g, 16))
        except:
          logger.error("Invalid formatting of Guid. Expected {01234567-89AB-CDEF-ABCD-0123456789AB}, got " + \
                       unicode(xmlvalue.firstChild.data))
          tmp = [0,0,0,0,0]
      if len(tmp) != 5:
        logger.error("Invalid formatting of Guid. Expected {01234567-89AB-CDEF-ABCD-0123456789AB}, got " + \
                     unicode(xmlvalue.firstChild.data))
        tmp = [0,0,0,0]
      self.value = tmp
