/*
 * qmqtt_network.cpp - qmqtt network
 *
 * Copyright (c) 2013  Ery Lee <ery.lee at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of mqttc nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */
#include <QDataStream>
#include "qmqtt_network.h"
#include "qmqtt_socket.h"

const QHostAddress DEFAULT_HOST = QHostAddress::LocalHost;
const quint16 DEFAULT_PORT = 1883;
const bool DEFAULT_AUTORECONNECT = false;

QMQTT::Network::Network(QObject* parent)
    : NetworkInterface(parent)
    , _port(DEFAULT_PORT)
    , _host(DEFAULT_HOST)
    , _socket(new QMQTT::Socket)
    , _autoReconnect(DEFAULT_AUTORECONNECT)
{
    initialize();
}

QMQTT::Network::Network(SocketInterface* socketInterface, QObject* parent)
    : NetworkInterface(parent)
    , _port(DEFAULT_PORT)
    , _host(DEFAULT_HOST)
    , _socket(socketInterface)
    , _autoReconnect(DEFAULT_AUTORECONNECT)
{
    initialize();
}

void QMQTT::Network::initialize()
{
    _socket->setParent(this);
    _buffer.open(QIODevice::ReadWrite);

    QObject::connect(_socket, &SocketInterface::connected, this, &Network::connected);
    QObject::connect(_socket, &SocketInterface::disconnected, this, &Network::disconnected);
    QObject::connect(_socket, &SocketInterface::readyRead, this, &Network::onSocketReadReady);
}

QMQTT::Network::~Network()
{
}

bool QMQTT::Network::isConnectedToHost() const
{
    return _socket->state() == QAbstractSocket::ConnectedState;
}

void QMQTT::Network::connectToHost(const QHostAddress& host, const quint16 port)
{
    _host = host;
    _port = port;
    _socket->connectToHost(host, port);
}

void QMQTT::Network::sendFrame(Frame& frame)
{
    if(_socket->state() == QAbstractSocket::ConnectedState)
    {
        QDataStream out(_socket);
        frame.write(out);
    }
}

void QMQTT::Network::disconnectFromHost()
{
    _socket->disconnectFromHost();
}

QAbstractSocket::SocketState QMQTT::Network::state() const
{
    return _socket->state();
}

bool QMQTT::Network::autoReconnect() const
{
    return _autoReconnect;
}

void QMQTT::Network::setAutoReconnect(const bool autoReconnect)
{
    _autoReconnect = autoReconnect;
}

void QMQTT::Network::onSocketReadReady()
{
    quint8 header = 0;
    int bytesRemaining = 0;
    int bytesRead = 0;

    QDataStream in(_socket);
    QDataStream out(&_buffer);
    while(!_socket->atEnd())
    {
        if(bytesRemaining == 0)
        {
            in >> header;
            bytesRemaining = readRemainingLength(in);
        }

        QByteArray data;
        data.resize(bytesRemaining);
        bytesRead = in.readRawData(data.data(), data.size());
        bytesRemaining -= bytesRead;
        out.writeRawData(data.data(), bytesRead);

        if(bytesRemaining == 0)
        {
            _buffer.reset();
            Frame frame(header, _buffer.buffer());
            _buffer.buffer().clear();
            emit received(frame);
        }
    }
}

int QMQTT::Network::readRemainingLength(QDataStream &in)
{
     qint8 byte = 0;
     int length = 0;
     int multiplier = 1;
     do {
         in >> byte;
         length += (byte & 127) * multiplier;
         multiplier *= 128;
     } while ((byte & 128) != 0);

     return length;
}
