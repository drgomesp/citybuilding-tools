/*
 * Citybuilding tools: a set of utilities to work with the game files
 * Copyright (c) 2017  Bianca van Schaik <bvschaik@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "textfilexmlstream.h"

#include "textfile.h"
#include "textgroup.h"


bool TextFileXmlStream::read(TextFile &file, QIODevice &device, Logger &logger)
{
    if (!device.open(QIODevice::ReadOnly)) {
        logger.error(QString("Unable to open XML file for reading: %1").arg(device.errorString()));
        return false;
    }
    
    QXmlStreamReader xml(&device);
    bool result = readFile(file, xml, logger);
    
    device.close();
    return result;
}

bool TextFileXmlStream::readFile(TextFile &file, QXmlStreamReader &xml, Logger &logger)
{
    if (!readOpenTag(xml, "strings", logger)) {
        logger.error("Unable to find root <strings> element");
        return false;
    }
    if (xml.attributes().hasAttribute("name")) {
        file.m_name = xml.attributes().value("name").toString();
    }
    if (xml.attributes().hasAttribute("indexWithCounts")) {
        file.m_indexWithCounts = xml.attributes().value("indexWithCounts").toString() != "false";
    }
    while (readOpenTag(xml, "group", logger)) {
        if (!readGroup(file, xml, logger) || !readCloseTag(xml, "group", logger)) {
            return false;
        }
    }
    return readCloseTag(xml, "strings", logger);
}

bool TextFileXmlStream::readOpenTag(QXmlStreamReader &xml, const QString &tag, Logger &logger)
{
    if (xml.isStartElement() && xml.name() == tag) {
        return true;
    }
    while (!xml.atEnd()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        switch (token) {
            case QXmlStreamReader::StartDocument:
            case QXmlStreamReader::Comment:
            case QXmlStreamReader::DTD:
            case QXmlStreamReader::Characters:
            case QXmlStreamReader::EntityReference:
            case QXmlStreamReader::ProcessingInstruction:
            case QXmlStreamReader::NoToken:
                continue;
                
            case QXmlStreamReader::Invalid:
                logger.error(QString("Invalid XML: %s").arg(xml.errorString()));
                return false;
            case QXmlStreamReader::EndDocument:
            case QXmlStreamReader::EndElement:
                return false;
                
            case QXmlStreamReader::StartElement:
                if (xml.name() == tag) {
                    return true;
                } else {
                    logger.error(QString("Invalid XML: expected tag <%1>, got <%2>")
                            .arg(tag, xml.name().toString()));
                    return false;
                }
        }
    }
    logger.error("Invalid XML: unexpected end of file");
    return false;
}

bool TextFileXmlStream::readCloseTag(QXmlStreamReader &xml, const QString &tag, Logger &logger)
{
    if (xml.isEndElement() && xml.name() == tag) {
        return true;
    }
    while (!xml.atEnd()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        if (token == QXmlStreamReader::EndElement && xml.name() == tag) {
            return true;
        }
    }
    logger.error(QString("Invalid XML: end element </%1> not found").arg(tag));
    return false;
}

bool TextFileXmlStream::readGroup(TextFile &file, QXmlStreamReader &xml, Logger &logger)
{
    if (!xml.attributes().hasAttribute("id")) {
        logger.error("Group does not have an ID attribute");
        return false;
    }
    bool ok;
    QString idString = xml.attributes().value("id").toString();
    int id = idString.toInt(&ok);
    if (!ok) {
        logger.error(QString("Group ID is not an integer: %1").arg(idString));
        return false;
    }
    TextGroup group(id);
    int index = 0;
    while (readOpenTag(xml, "string", logger)) {
        QString textIdString = xml.attributes().value("id").toString();
        int textId = textIdString.toInt(&ok);
        if (!ok) {
            logger.error(QString("String ID is not an integer: %1").arg(textIdString));
            return false;
        }
        if (textId != group.size()) {
            logger.error(QString("Strings in group %1 are not ordered properly").arg(id));
            return false;
        }
        index++;
        group.add(xml.readElementText());
        readCloseTag(xml, "string", logger);
    }
    file.m_groups.append(group);
    return true;
}

bool TextFileXmlStream::write(TextFile &file, QIODevice &device, Logger &logger)
{
    if (!device.open(QIODevice::WriteOnly)) {
        logger.error(QString("Unable to open XML file for writing: %1").arg(device.errorString()));
        return false;
    }
    QXmlStreamWriter xml(&device);
    xml.setAutoFormatting(true);
    
    xml.writeStartDocument();
    xml.writeStartElement("strings");
    xml.writeAttribute("name", file.m_name);
    xml.writeAttribute("indexWithCounts", file.m_indexWithCounts ? "true" : "false");
    
    QList<TextGroup>::const_iterator it;
    for (it = file.m_groups.constBegin(); it != file.m_groups.constEnd(); it++) {
        writeGroup(*it, xml);
    }
    
    xml.writeEndElement();
    xml.writeEndDocument();
    
    device.close();
    return true;
}

void TextFileXmlStream::writeGroup(const TextGroup &group, QXmlStreamWriter &xml)
{
    xml.writeStartElement("group");
    xml.writeAttribute("id", QString::number(group.id()));

    const QStringList &strings = group.strings();
    QStringList::const_iterator it;
    int index = 0;
    for (it = strings.constBegin(); it != strings.constEnd(); it++) {
        xml.writeStartElement("string");
        xml.writeAttribute("id", QString::number(index));
        xml.writeCharacters(*it);
        xml.writeEndElement();
        index++;
    }

    xml.writeEndElement();
}
