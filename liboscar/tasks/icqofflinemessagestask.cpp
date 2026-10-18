/*
    harbour_icq_nina
    icqofflinemessagestask.cpp - ICQ offline messages over the ICQ meta family

    This library is free software; you can redistribute it and/or modify it
    under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License, or (at your
    option) any later version.
*/
#include "icqofflinemessagestask.h"

#include <QDateTime>
#include <QTextCodec>

#include <kdebug.h>

#include "buffer.h"
#include "connection.h"
#include "transfer.h"

ICQOfflineMessagesTask::ICQOfflineMessagesTask( Task* parent )
	: ICQTask( parent ), m_goSequence( 0 )
{
}

QString ICQOfflineMessagesTask::decodeLegacyText( const QByteArray &bytes )
{
	QTextCodec::ConverterState state;
	QTextCodec *utf8 = QTextCodec::codecForName( "UTF-8" );
	const QString text = utf8->toUnicode( bytes.constData(), bytes.size(), &state );
	if ( state.invalidChars == 0 )
		return text;
	if ( QTextCodec *cp1252 = QTextCodec::codecForName( "windows-1252" ) )
		return cp1252->toUnicode( bytes );
	return QString::fromLatin1( bytes );
}

bool ICQOfflineMessagesTask::forMe( const Transfer* transfer ) const
{
	const SnacTransfer* st = dynamic_cast<const SnacTransfer*>( transfer );
	if ( !st || st->snacService() != 0x0015 || st->snacSubtype() != 0x0003 )
		return false;

	Buffer buf( *( st->buffer() ) );
	const_cast<ICQOfflineMessagesTask*>( this )->parseInitialData( buf );
	return requestType() == 0x0041 || requestType() == 0x0042;
}

bool ICQOfflineMessagesTask::take( Transfer* transfer )
{
	if ( !forMe( transfer ) )
		return false;

	setTransfer( transfer );
	TLV tlv1 = transfer->buffer()->getTLV();
	Buffer data( tlv1.data, tlv1.length );
	data.skipBytes( 10 ); // chunk size, uin, request type, sequence

	if ( requestType() == 0x0041 )
	{
		handleRecord( data );
	}
	else
	{
		kDebug( OSCAR_RAW_DEBUG ) << "End of ICQ offline messages";
		deleteMessages();
		setSuccess( 0, QString() );
	}
	setTransfer( 0 );
	return true;
}

void ICQOfflineMessagesTask::handleRecord( Buffer &data )
{
	if ( data.bytesAvailable() < 14 )
		return;

	const Oscar::DWORD senderUin = data.getLEDWord();
	const int year = data.getLEWord();
	const int month = data.getByte();
	const int day = data.getByte();
	const int hour = data.getByte();
	const int minute = data.getByte();
	const int type = data.getByte();
	data.getByte(); // flags
	const QByteArray raw = data.getLELNTS();

	const QString sender = QString::number( senderUin );
	QDateTime when( QDate( year, month, day ), QTime( hour, minute ), Qt::UTC );
	if ( !when.isValid() )
		when = QDateTime::currentDateTime();

	// ICQ separates the fields of structured messages with 0xFE.
	const QList<QByteArray> fields = raw.split( char( 0xFE ) );

	switch ( type )
	{
	case 0x01: // plain text
	case 0x04: // URL: description 0xFE url
	{
		QString text = decodeLegacyText( type == 0x04 ? fields.value( 0 ) : raw );
		if ( type == 0x04 && fields.size() > 1 )
			text = ( text.isEmpty() ? QString() : text + QLatin1Char( ' ' ) ) + decodeLegacyText( fields.value( 1 ) );

		Oscar::Message msg;
		msg.setSender( sender );
		msg.setReceiver( client()->userId() );
		msg.setChannel( 0x01 );
		msg.setTimestamp( when.toLocalTime() );
		msg.setText( Oscar::Message::UCS2, text );
		emit offlineMessage( msg );
		break;
	}
	case 0x06: // authorization request: nick, first, last, email, auth, reason
		emit offlineAuthRequest( sender, decodeLegacyText( fields.value( 5 ) ) );
		break;
	case 0x07: // authorization denied: reason
		emit offlineAuthReply( sender, decodeLegacyText( raw ), false );
		break;
	case 0x08: // authorization granted
		emit offlineAuthReply( sender, QString(), true );
		break;
	default:
		kDebug( OSCAR_RAW_DEBUG ) << "Ignoring offline message of type" << type;
		break;
	}
}

void ICQOfflineMessagesTask::onGo()
{
	kDebug( OSCAR_RAW_DEBUG ) << "Requesting ICQ offline messages";
	setSequence( client()->snacSequence() );
	setRequestType( 0x003C );
	setRequestSubType( 0xFFFF );

	m_goSequence = client()->snacSequence();
	Buffer *sendBuf = addInitialData();
	FLAP f = { 0x02, 0, 0 };
	SNAC s = { 0x0015, 0x0002, 0, m_goSequence };
	send( createTransfer( f, s, sendBuf ) );
}

void ICQOfflineMessagesTask::deleteMessages()
{
	setSequence( client()->snacSequence() );
	setRequestType( 0x003E );
	setRequestSubType( 0xFFFF );

	Buffer *sendBuf = addInitialData();
	FLAP f = { 0x02, 0, 0 };
	SNAC s = { 0x0015, 0x0002, 0, client()->snacSequence() };
	send( createTransfer( f, s, sendBuf ) );
}
