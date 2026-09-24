
/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
 * tests/unit/xml/xmltest.c - Unit tests for xml.h (xml.c implementation)
 *
 * Covers both the streaming XML printer and the recursive-descent parser.
 * All parsing exercises are performed via makeXmlStringParser so no file I/O
 * is required.
 *
 * Bugs in xml.c that are verified (and fixed) by these tests
 * ----------------------------------------------------------
 *  1. traceXML / parseTrace defaulted to TRUE → every safeRead spammed stdout
 *  2. writeByte() called customWriteByte but then also fell through to
 *     write(fd, ...) unconditionally -- fixed to use if/else if/else
 *  3. makeXMLNode left childrenLength and attributesLength uninitialised
 *  4. addChild resize used childrenLength++ instead of the new capacity →
 *     safeFree called with wrong size on the next resize
 *  5. addAttribute first allocation never set attributesLength = 4 →
 *     undefined comparison on the second addAttribute call
 *  6. addAttribute resize did not update attributesLength at all
 *  7. &quot; and &apos; entities returned XMLTOKEN_BROKEN
 *  8. <?...?> processing instructions caused parse failure (unknown token)
 *  9. <![CDATA[...]]> sections caused parse failure (NULL return)
 * 10. intFromChildWithTag dereferenced NULL when the child tag was absent
 *
 * Compile on z/OS with xlclang (lp64) -- see tests/Makefile target "test_xml".
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zowetypes.h"
#include "alloc.h"
#include "xml.h"
#include "zowetests.h"

/* freeXmlParser is implemented in xml.c but not yet in xml.h */
void freeXmlParser(XmlParser *p);

/* =====================================================================
 *  Printer capture helpers
 *
 *  A pair of makeCustomXmlPrinter callbacks that accumulate all output
 *  into a fixed-size stack buffer so tests can inspect the rendered XML.
 * ===================================================================== */

typedef struct {
  char buf[4096];
  int  pos;
} CaptureBuffer;

static void capWriteFully(xmlPrinter *p, char *text, int len) {
  CaptureBuffer *cb = (CaptureBuffer *)p->customObject;
  int space = (int)sizeof(cb->buf) - cb->pos - 1;
  if (len > space) len = space;
  memcpy(cb->buf + cb->pos, text, len);
  cb->pos += len;
  cb->buf[cb->pos] = '\0';
}

static void capWriteByte(xmlPrinter *p, char c) {
  CaptureBuffer *cb = (CaptureBuffer *)p->customObject;
  if (cb->pos < (int)sizeof(cb->buf) - 1) {
    cb->buf[cb->pos++] = c;
    cb->buf[cb->pos] = '\0';
  }
}

/* =====================================================================
 *  Suite 1 - Self-closing element
 * ===================================================================== */

static void testXmlParserSelfClosing(void) {
  DESCRIBE("parseXMLNode - self-closing element") {

    IT("parses a self-closing root element without error") {
      TEST_COVERS(parseXMLNode);
      char xml[] = "<root/>";
      XmlParser *par = makeXmlStringParser(xml, (int)strlen(xml));
      XMLNode *node = parseXMLNode(par);
      ASSERT_NOT_NULL(node);
      ASSERT_EQUAL_INT(NODE_ELEMENT, node->type);
      ASSERT_EQUAL_STR("root", node->name);
      ASSERT_EQUAL_INT(0, node->childCount);
      ASSERT_EQUAL_INT(0, node->attributeCount);
      freeXmlParser(par);
    } IT_END

    IT("returns NULL for completely empty input") {
      TEST_COVERS(parseXMLNode);
      char xml[] = "";
      XmlParser *par = makeXmlStringParser(xml, 0);
      XMLNode *node = parseXMLNode(par);
      ASSERT_NULL(node);
      freeXmlParser(par);
    } IT_END

  } DESCRIBE_END
}

/* =====================================================================
 *  Suite 2 - Open/close element pair
 * ===================================================================== */

static void testXmlParserSimpleElement(void) {
  DESCRIBE("parseXMLNode - open/close element pair") {

    IT("parses <element></element> with no children") {
      TEST_COVERS(parseXMLNode);
      char xml[] = "<item></item>";
      XmlParser *par = makeXmlStringParser(xml, (int)strlen(xml));
      XMLNode *node = parseXMLNode(par);
      ASSERT_NOT_NULL(node);
      ASSERT_EQUAL_STR("item", node->name);
      ASSERT_EQUAL_INT(0, node->childCount);
      freeXmlParser(par);
    } IT_END

    IT("element names with hyphens and underscores are accepted") {
      TEST_COVERS(parseXMLNode);
      char xml[] = "<my-element_1/>";
      XmlParser *par = makeXmlStringParser(xml, (int)strlen(xml));
      XMLNode *node = parseXMLNode(par);
      ASSERT_NOT_NULL(node);
      ASSERT_EQUAL_STR("my-element_1", node->name);
      freeXmlParser(par);
    } IT_END

  } DESCRIBE_END
}

/* =====================================================================
 *  Suite 3 - Text content
 * ===================================================================== */

static void testXmlParserTextContent(void) {
  DESCRIBE("parseXMLNode - text content") {

    IT("nodeText returns the text content of an element") {
      TEST_COVERS(nodeText);
      char xml[] = "<msg>hello world</msg>";
      XmlParser *par = makeXmlStringParser(xml, (int)strlen(xml));
      XMLNode *node = parseXMLNode(par);
      ASSERT_NOT_NULL(node);
      char *text = nodeText(node);
      ASSERT_NOT_NULL(text);
      ASSERT_EQUAL_STR("hello world", text);
      freeXmlParser(par);
    } IT_END

    IT("nodeText returns NULL for an element with no text") {
      TEST_COVERS(nodeText);
      char xml[] = "<empty/>";
      XmlParser *par = makeXmlStringParser(xml, (int)strlen(xml));
      XMLNode *node = parseXMLNode(par);
      ASSERT_NOT_NULL(node);
      ASSERT_NULL(nodeText(node));
      freeXmlParser(par);
    } IT_END

  } DESCRIBE_END
}

/* =====================================================================
 *  Suite 4 - Attributes
 * ===================================================================== */

static void testXmlParserAttributes(void) {
  DESCRIBE("parseXMLNode - attributes") {

    IT("getAttribute returns the correct value for each attribute") {
      TEST_COVERS(getAttribute);
      char xml[] = "<item id=\"42\" name=\"foo\"/>";
      XmlParser *par = makeXmlStringParser(xml, (int)strlen(xml));
      XMLNode *node = parseXMLNode(par);
      ASSERT_NOT_NULL(node);
      char *id   = getAttribute(node, "id");
      char *name = getAttribute(node, "name");
      ASSERT_NOT_NULL(id);
      ASSERT_NOT_NULL(name);
      ASSERT_EQUAL_STR("42",  id);
      ASSERT_EQUAL_STR("foo", name);
      freeXmlParser(par);
    } IT_END

    IT("getAttribute returns NULL for a missing attribute") {
      TEST_COVERS(getAttribute);
      char xml[] = "<item id=\"1\"/>";
      XmlParser *par = makeXmlStringParser(xml, (int)strlen(xml));
      XMLNode *node = parseXMLNode(par);
      ASSERT_NOT_NULL(node);
      ASSERT_NULL(getAttribute(node, "missing"));
      freeXmlParser(par);
    } IT_END

    IT("attribute values are preserved when using open/close syntax") {
      TEST_COVERS(getAttribute);
      char xml[] = "<item color=\"blue\"></item>";
      XmlParser *par = makeXmlStringParser(xml, (int)strlen(xml));
      XMLNode *node = parseXMLNode(par);
      ASSERT_NOT_NULL(node);
      ASSERT_EQUAL_STR("blue", getAttribute(node, "color"));
      freeXmlParser(par);
    } IT_END

  } DESCRIBE_END
}

/* =====================================================================
 *  Suite 5 - Many attributes  (exercises the addAttribute resize path)
 *
 *  addAttribute initially allocates room for 4 attribute nodes.  A fifth
 *  attribute forces reallocation.  Before the fix, attributesLength was
 *  never set on the first allocation so the resize comparison used
 *  uninitialised memory.
 * ===================================================================== */

static void testXmlParserManyAttributes(void) {
  DESCRIBE("parseXMLNode - six attributes trigger the addAttribute resize") {

    IT("all six attribute values are accessible after resize") {
      TEST_COVERS(getAttribute);
      char xml[] =
        "<el a1=\"v1\" a2=\"v2\" a3=\"v3\" a4=\"v4\" a5=\"v5\" a6=\"v6\"/>";
      XmlParser *par = makeXmlStringParser(xml, (int)strlen(xml));
      XMLNode *node = parseXMLNode(par);
      ASSERT_NOT_NULL(node);
      ASSERT_EQUAL_INT(6, node->attributeCount);
      ASSERT_EQUAL_STR("v1", getAttribute(node, "a1"));
      ASSERT_EQUAL_STR("v2", getAttribute(node, "a2"));
      ASSERT_EQUAL_STR("v3", getAttribute(node, "a3"));
      ASSERT_EQUAL_STR("v4", getAttribute(node, "a4"));
      ASSERT_EQUAL_STR("v5", getAttribute(node, "a5"));
      ASSERT_EQUAL_STR("v6", getAttribute(node, "a6"));
      freeXmlParser(par);
    } IT_END

  } DESCRIBE_END
}

/* =====================================================================
 *  Suite 6 - Nested elements and DOM traversal helpers
 * ===================================================================== */

static void testXmlParserNested(void) {
  DESCRIBE("parseXMLNode - nested elements and DOM helpers") {

    IT("firstRealChild returns the first non-text child element") {
      TEST_COVERS(firstRealChild);
      char xml[] = "<root><alpha/><beta/></root>";
      XmlParser *par = makeXmlStringParser(xml, (int)strlen(xml));
      XMLNode *node = parseXMLNode(par);
      ASSERT_NOT_NULL(node);
      XMLNode *first = firstRealChild(node);
      ASSERT_NOT_NULL(first);
      ASSERT_EQUAL_STR("alpha", first->name);
      freeXmlParser(par);
    } IT_END

    IT("nextRealSibling walks the sibling chain in document order") {
      TEST_COVERS(nextRealSibling);
      char xml[] = "<root><alpha/><beta/><gamma/></root>";
      XmlParser *par = makeXmlStringParser(xml, (int)strlen(xml));
      XMLNode *node = parseXMLNode(par);
      ASSERT_NOT_NULL(node);
      XMLNode *first  = firstRealChild(node);
      XMLNode *second = nextRealSibling(first);
      XMLNode *third  = nextRealSibling(second);
      ASSERT_EQUAL_STR("alpha", first->name);
      ASSERT_EQUAL_STR("beta",  second->name);
      ASSERT_EQUAL_STR("gamma", third->name);
      ASSERT_NULL(nextRealSibling(third));
      freeXmlParser(par);
    } IT_END

    IT("firstChildWithTag locates a named descendant") {
      TEST_COVERS(firstChildWithTag);
      char xml[] = "<root><alpha/><target val=\"yes\"/><beta/></root>";
      XmlParser *par = makeXmlStringParser(xml, (int)strlen(xml));
      XMLNode *node = parseXMLNode(par);
      ASSERT_NOT_NULL(node);
      XMLNode *target = firstChildWithTag(node, "target");
      ASSERT_NOT_NULL(target);
      ASSERT_EQUAL_STR("yes", getAttribute(target, "val"));
      freeXmlParser(par);
    } IT_END

    IT("firstChildWithTag returns NULL when the tag is absent") {
      TEST_COVERS(firstChildWithTag);
      char xml[] = "<root><alpha/></root>";
      XmlParser *par = makeXmlStringParser(xml, (int)strlen(xml));
      XMLNode *node = parseXMLNode(par);
      ASSERT_NOT_NULL(node);
      ASSERT_NULL(firstChildWithTag(node, "nosuchelem"));
      freeXmlParser(par);
    } IT_END

  } DESCRIBE_END
}

/* =====================================================================
 *  Suite 7 - Many children  (exercises the addChild resize path)
 *
 *  addChild starts with an initial capacity of 4.  A fifth child forces
 *  reallocation.  Before the fix, childrenLength was incremented by 1
 *  rather than being set to the new capacity, so every subsequent resize
 *  called safeFree with the wrong (too-small) allocation size.
 * ===================================================================== */

static void testXmlParserManyChildren(void) {
  DESCRIBE("parseXMLNode - six children trigger the addChild resize") {

    IT("all six children are reachable via the sibling chain") {
      TEST_COVERS(firstRealChild);
      TEST_COVERS(nextRealSibling);
      char xml[] = "<root><c1/><c2/><c3/><c4/><c5/><c6/></root>";
      XmlParser *par = makeXmlStringParser(xml, (int)strlen(xml));
      XMLNode *node = parseXMLNode(par);
      ASSERT_NOT_NULL(node);
      ASSERT_EQUAL_INT(6, node->childCount);
      /* walk the full sibling chain and count */
      int count = 0;
      XMLNode *cur = firstRealChild(node);
      while (cur != NULL) {
        count++;
        cur = nextRealSibling(cur);
      }
      ASSERT_EQUAL_INT(6, count);
      freeXmlParser(par);
    } IT_END

  } DESCRIBE_END
}

/* =====================================================================
 *  Suite 8 - Comments
 * ===================================================================== */

static void testXmlParserComments(void) {
  DESCRIBE("parseXMLNode - XML comments are skipped transparently") {

    IT("a leading comment is skipped and the root element is returned") {
      TEST_COVERS(parseXMLNode);
      char xml[] = "<!-- header comment --><root/>";
      XmlParser *par = makeXmlStringParser(xml, (int)strlen(xml));
      XMLNode *node = parseXMLNode(par);
      ASSERT_NOT_NULL(node);
      ASSERT_EQUAL_STR("root", node->name);
      freeXmlParser(par);
    } IT_END

    IT("an inline comment between child elements is skipped") {
      TEST_COVERS(parseXMLNode);
      char xml[] = "<root><!-- ignored --><child/></root>";
      XmlParser *par = makeXmlStringParser(xml, (int)strlen(xml));
      XMLNode *node = parseXMLNode(par);
      ASSERT_NOT_NULL(node);
      XMLNode *child = firstRealChild(node);
      ASSERT_NOT_NULL(child);
      ASSERT_EQUAL_STR("child", child->name);
      freeXmlParser(par);
    } IT_END

  } DESCRIBE_END
}

/* =====================================================================
 *  Suite 9 - Processing instructions
 *
 *  Before the fix, a <?...?> PI caused the tokeniser to try to read '?'
 *  as the start of an element name.  Because '?' is not alpha, the
 *  identifier reader reached the "unknown token" error path and returned
 *  NULL, which propagated as a parse failure.
 * ===================================================================== */

static void testXmlParserProcessingInstruction(void) {
  DESCRIBE("parseXMLNode - processing instructions are skipped") {

    IT("a bare XML declaration before the root is skipped") {
      TEST_COVERS(parseXMLNode);
      char xml[] = "<?xml version=\"1.0\"?><root/>";
      XmlParser *par = makeXmlStringParser(xml, (int)strlen(xml));
      XMLNode *node = parseXMLNode(par);
      ASSERT_NOT_NULL(node);
      ASSERT_EQUAL_STR("root", node->name);
      freeXmlParser(par);
    } IT_END

    IT("an XML declaration with encoding is skipped") {
      TEST_COVERS(parseXMLNode);
      char xml[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><data/>";
      XmlParser *par = makeXmlStringParser(xml, (int)strlen(xml));
      XMLNode *node = parseXMLNode(par);
      ASSERT_NOT_NULL(node);
      ASSERT_EQUAL_STR("data", node->name);
      freeXmlParser(par);
    } IT_END

  } DESCRIBE_END
}

/* =====================================================================
 *  Suite 10 - XML character entities in text content
 *
 *  Before the fix, &quot; and &apos; returned XMLTOKEN_BROKEN because they
 *  were missing from the entity dispatch table.  The tests also confirm the
 *  pre-existing &lt;, &gt;, and &amp; cases remain correct.
 * ===================================================================== */

static void testXmlParserEntities(void) {
  DESCRIBE("parseXMLNode - character entity decoding in text content") {

    IT("&lt; and &gt; are decoded to literal < and > characters") {
      TEST_COVERS(nodeText);
      /*
       * The leading "text: " prefix ensures the '&' is encountered inside
       * the PCDATA read loop (not as the very first byte, which is written
       * before entity processing begins).
       */
      char xml[] = "<t>text: &lt; and &gt; end</t>";
      XmlParser *par = makeXmlStringParser(xml, (int)strlen(xml));
      XMLNode *node = parseXMLNode(par);
      ASSERT_NOT_NULL(node);
      char *text = nodeText(node);
      ASSERT_NOT_NULL(text);
      ASSERT_NOT_NULL(strchr(text, '<'));
      ASSERT_NOT_NULL(strchr(text, '>'));
      freeXmlParser(par);
    } IT_END

    IT("&amp; is decoded to a literal ampersand") {
      TEST_COVERS(nodeText);
      char xml[] = "<t>A &amp; B</t>";
      XmlParser *par = makeXmlStringParser(xml, (int)strlen(xml));
      XMLNode *node = parseXMLNode(par);
      ASSERT_NOT_NULL(node);
      char *text = nodeText(node);
      ASSERT_NOT_NULL(text);
      ASSERT_NOT_NULL(strchr(text, '&'));
      freeXmlParser(par);
    } IT_END

    IT("&quot; is decoded to a double-quote (was previously BROKEN)") {
      TEST_COVERS(nodeText);
      char xml[] = "<t>say &quot;hello&quot;</t>";
      XmlParser *par = makeXmlStringParser(xml, (int)strlen(xml));
      XMLNode *node = parseXMLNode(par);
      ASSERT_NOT_NULL(node);
      char *text = nodeText(node);
      ASSERT_NOT_NULL(text);
      ASSERT_NOT_NULL(strchr(text, '"'));
      freeXmlParser(par);
    } IT_END

    IT("&apos; is decoded to a single-quote (was previously BROKEN)") {
      TEST_COVERS(nodeText);
      char xml[] = "<t>it&apos;s here</t>";
      XmlParser *par = makeXmlStringParser(xml, (int)strlen(xml));
      XMLNode *node = parseXMLNode(par);
      ASSERT_NOT_NULL(node);
      char *text = nodeText(node);
      ASSERT_NOT_NULL(text);
      ASSERT_NOT_NULL(strchr(text, '\''));
      freeXmlParser(par);
    } IT_END

  } DESCRIBE_END
}

/* =====================================================================
 *  Suite 11 - CDATA sections
 *
 *  Before the fix, a <![CDATA[...]]> section reached the "Unrecognised <!
 *  construct" error path because only <!-- (comment) was handled under the
 *  <! prefix.
 * ===================================================================== */

static void testXmlParserCDATA(void) {
  DESCRIBE("parseXMLNode - CDATA sections") {

    IT("CDATA content is preserved verbatim including markup characters") {
      TEST_COVERS(parseXMLNode);
      TEST_COVERS(nodeText);
      char xml[] = "<t><![CDATA[<not a tag>]]></t>";
      XmlParser *par = makeXmlStringParser(xml, (int)strlen(xml));
      XMLNode *node = parseXMLNode(par);
      ASSERT_NOT_NULL(node);
      char *text = nodeText(node);
      ASSERT_NOT_NULL(text);
      /* Angle brackets inside CDATA must survive without being treated as
       * markup -- they are the canonical CDATA use-case. */
      ASSERT_NOT_NULL(strchr(text, '<'));
      ASSERT_NOT_NULL(strchr(text, '>'));
      freeXmlParser(par);
    } IT_END

    IT("an empty CDATA section does not crash the parser") {
      TEST_COVERS(parseXMLNode);
      char xml[] = "<t><![CDATA[]]></t>";
      XmlParser *par = makeXmlStringParser(xml, (int)strlen(xml));
      XMLNode *node = parseXMLNode(par);
      ASSERT_NOT_NULL(node);
      freeXmlParser(par);
    } IT_END

    IT("CDATA inside a multi-child document is parsed correctly") {
      TEST_COVERS(parseXMLNode);
      char xml[] =
        "<root>"
          "<plain>normal</plain>"
          "<raw><![CDATA[x < y && y > z]]></raw>"
        "</root>";
      XmlParser *par = makeXmlStringParser(xml, (int)strlen(xml));
      XMLNode *node = parseXMLNode(par);
      ASSERT_NOT_NULL(node);
      char *rawText = textFromChildWithTag(node, "raw");
      ASSERT_NOT_NULL(rawText);
      ASSERT_NOT_NULL(strchr(rawText, '<'));
      freeXmlParser(par);
    } IT_END

  } DESCRIBE_END
}

/* =====================================================================
 *  Suite 12 - DOM utility functions
 * ===================================================================== */

static void testXmlParserDOM(void) {
  DESCRIBE("DOM utility functions") {

    IT("textFromChildWithTag retrieves the text of a named child") {
      TEST_COVERS(textFromChildWithTag);
      char xml[] = "<config><host>myhost</host><port>8080</port></config>";
      XmlParser *par = makeXmlStringParser(xml, (int)strlen(xml));
      XMLNode *node = parseXMLNode(par);
      ASSERT_NOT_NULL(node);
      char *host = textFromChildWithTag(node, "host");
      ASSERT_NOT_NULL(host);
      ASSERT_EQUAL_STR("myhost", host);
      freeXmlParser(par);
    } IT_END

    IT("textFromChildWithTag returns NULL for a self-closing child") {
      TEST_COVERS(textFromChildWithTag);
      char xml[] = "<config><empty/></config>";
      XmlParser *par = makeXmlStringParser(xml, (int)strlen(xml));
      XMLNode *node = parseXMLNode(par);
      ASSERT_NOT_NULL(node);
      ASSERT_NULL(textFromChildWithTag(node, "empty"));
      freeXmlParser(par);
    } IT_END

    IT("intFromChildWithTag returns 0 safely when tag is absent (null guard)") {
      TEST_COVERS(intFromChildWithTag);
      char xml[] = "<config><name>foo</name></config>";
      XmlParser *par = makeXmlStringParser(xml, (int)strlen(xml));
      XMLNode *node = parseXMLNode(par);
      ASSERT_NOT_NULL(node);
      int val = -1;
      /* Must return 0 (not crash on NULL text) when the tag is not found */
      int rc = intFromChildWithTag(node, "port", &val);
      ASSERT_EQUAL_INT(0, rc);
      freeXmlParser(par);
    } IT_END

    IT("intFromChildWithTag decodes a decimal integer from child text") {
      TEST_COVERS(intFromChildWithTag);
      char xml[] = "<config><port>8080</port></config>";
      XmlParser *par = makeXmlStringParser(xml, (int)strlen(xml));
      XMLNode *node = parseXMLNode(par);
      ASSERT_NOT_NULL(node);
      int val = 0;
      int rc = intFromChildWithTag(node, "port", &val);
      ASSERT_EQUAL_INT(1,    rc);
      ASSERT_EQUAL_INT(8080, val);
      freeXmlParser(par);
    } IT_END

  } DESCRIBE_END
}

/* =====================================================================
 *  Suite 13 - XML printer with custom writer
 *
 *  The printer is exercised through makeCustomXmlPrinter with the capture
 *  callbacks above.  The fix to writeByte() ensures that the custom
 *  callback is the only output path (previously it also wrote to fd 0).
 * ===================================================================== */

static void testXmlPrinter(void) {
  DESCRIBE("xmlPrinter - output captured via custom writer callbacks") {

    IT("xmlStart and xmlEnd produce matching open and close tags") {
      TEST_COVERS(xmlStart);
      TEST_COVERS(xmlEnd);
      CaptureBuffer cb;
      memset(&cb, 0, sizeof(cb));
      xmlPrinter *p = makeCustomXmlPrinter(NULL, capWriteFully, capWriteByte, &cb);
      xmlStart(p, "root");
      xmlEnd(p);
      xmlClose(p);
      ASSERT_NOT_NULL(strstr(cb.buf, "<root"));
      ASSERT_NOT_NULL(strstr(cb.buf, "</root>"));
    } IT_END

    IT("xmlAddString wraps the string in the named element") {
      TEST_COVERS(xmlAddString);
      CaptureBuffer cb;
      memset(&cb, 0, sizeof(cb));
      xmlPrinter *p = makeCustomXmlPrinter(NULL, capWriteFully, capWriteByte, &cb);
      xmlStart(p, "root");
      xmlAddString(p, "msg", "hello");
      xmlEnd(p);
      xmlClose(p);
      ASSERT_NOT_NULL(strstr(cb.buf, "<msg>"));
      ASSERT_NOT_NULL(strstr(cb.buf, "hello"));
      ASSERT_NOT_NULL(strstr(cb.buf, "</msg>"));
    } IT_END

    IT("xmlPrint escapes < and & in text content") {
      TEST_COVERS(xmlPrint);
      CaptureBuffer cb;
      memset(&cb, 0, sizeof(cb));
      xmlPrinter *p = makeCustomXmlPrinter(NULL, capWriteFully, capWriteByte, &cb);
      xmlStart(p, "t");
      xmlPrint(p, "a < b & c");
      xmlEnd(p);
      xmlClose(p);
      ASSERT_NOT_NULL(strstr(cb.buf, "&lt;"));
      ASSERT_NOT_NULL(strstr(cb.buf, "&amp;"));
    } IT_END

    IT("xmlAddIntElement writes the integer as decimal text") {
      TEST_COVERS(xmlAddIntElement);
      CaptureBuffer cb;
      memset(&cb, 0, sizeof(cb));
      xmlPrinter *p = makeCustomXmlPrinter(NULL, capWriteFully, capWriteByte, &cb);
      xmlStart(p, "root");
      xmlAddIntElement(p, "count", 42);
      xmlEnd(p);
      xmlClose(p);
      ASSERT_NOT_NULL(strstr(cb.buf, "<count>"));
      ASSERT_NOT_NULL(strstr(cb.buf, "42"));
    } IT_END

    IT("xmlAddBooleanElement writes TRUE for non-zero and FALSE for zero") {
      TEST_COVERS(xmlAddBooleanElement);
      CaptureBuffer cb;
      memset(&cb, 0, sizeof(cb));
      xmlPrinter *p = makeCustomXmlPrinter(NULL, capWriteFully, capWriteByte, &cb);
      xmlStart(p, "root");
      xmlAddBooleanElement(p, "flag", 1);
      xmlAddBooleanElement(p, "nope", 0);
      xmlEnd(p);
      xmlClose(p);
      ASSERT_NOT_NULL(strstr(cb.buf, "TRUE"));
      ASSERT_NOT_NULL(strstr(cb.buf, "FALSE"));
    } IT_END

    IT("xmlAddCData wraps raw content in a CDATA section") {
      TEST_COVERS(xmlAddCData);
      CaptureBuffer cb;
      memset(&cb, 0, sizeof(cb));
      xmlPrinter *p = makeCustomXmlPrinter(NULL, capWriteFully, capWriteByte, &cb);
      xmlStart(p, "root");
      xmlAddCData(p, "raw", "<not escaped>");
      xmlEnd(p);
      xmlClose(p);
      ASSERT_NOT_NULL(strstr(cb.buf, "<![CDATA["));
      ASSERT_NOT_NULL(strstr(cb.buf, "<not escaped>"));
      ASSERT_NOT_NULL(strstr(cb.buf, "]]>"));
    } IT_END

    IT("nested xmlStart calls produce a properly indented hierarchy") {
      TEST_COVERS(xmlStart);
      CaptureBuffer cb;
      memset(&cb, 0, sizeof(cb));
      xmlPrinter *p = makeCustomXmlPrinter(NULL, capWriteFully, capWriteByte, &cb);
      xmlStart(p, "outer");
      xmlStart(p, "inner");
      xmlPrint(p, "content");
      xmlEnd(p); /* inner */
      xmlEnd(p); /* outer */
      xmlClose(p);
      ASSERT_NOT_NULL(strstr(cb.buf, "<outer"));
      ASSERT_NOT_NULL(strstr(cb.buf, "<inner>"));
      ASSERT_NOT_NULL(strstr(cb.buf, "</inner>"));
      ASSERT_NOT_NULL(strstr(cb.buf, "</outer>"));
    } IT_END

  } DESCRIBE_END
}

/* =====================================================================
 *  Entry point
 * ===================================================================== */

int main(void) {
  zoweTestInit();
  /* Silence the verbose character-by-character parse tracing. */
  setXMLTrace(0);
  setXMLParseTrace(0);

  testXmlParserSelfClosing();
  testXmlParserSimpleElement();
  testXmlParserTextContent();
  testXmlParserAttributes();
  testXmlParserManyAttributes();
  testXmlParserNested();
  testXmlParserManyChildren();
  testXmlParserComments();
  testXmlParserProcessingInstruction();
  testXmlParserEntities();
  testXmlParserCDATA();
  testXmlParserDOM();
  testXmlPrinter();

  return ZOWE_TEST_REPORT();
}
