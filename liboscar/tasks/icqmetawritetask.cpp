/*
  icqmetawritetask.cpp - one classic ICQ directory write.

  Part of harbour_icq_nina (liboscar additions); LGPL-2.0-or-later.
*/
#include "icqmetawritetask.h"

#include <kdebug.h>

#include "buffer.h"
#include "connection.h"
#include "transfer.h"

ICQMetaWriteTask::ICQMetaWriteTask( Task* parent, Oscar::WORD subtype, const QByteArray& payload )
	: ICQTask( parent ), m_subtype( subtype ), m_payload( payload ), m_goSequence( 0 )
{
}

bool ICQMetaWriteTask::forMe( const Transfer* transfer ) const
{
	const SnacTransfer* st = dynamic_cast<const SnacTransfer*>( transfer );
	if ( !st || st->snacService() != 0x0015 || st->snacSubtype() != 0x0003 || st->snacRequest() != m_goSequence )
		return false;

	Buffer buf( *( st->buffer() ) );
	const_cast<ICQMetaWriteTask*>( this )->parseInitialData( buf );
	// The answer to our request (same SNAC id), whatever subtype the server
	// gives it (AOL: 0x0064 for 0x03EA, 0x0078 for 0x03FD, 0x0082 for 0x0406).
	return requestType() == 0x07DA;
}

bool ICQMetaWriteTask::take( Transfer* transfer )
{
	if ( !forMe( transfer ) )
		return false;
	setTransfer( transfer );
	TLV tlv1 = transfer->buffer()->getTLV();
	Buffer buffer( tlv1.data, tlv1.length );
	buffer.skipBytes( 12 ); // chunk size, uin, type, sequence, subtype
	const Oscar::BYTE result = buffer.bytesAvailable() > 0 ? buffer.getByte() : 0;
	kDebug(OSCAR_RAW_DEBUG) << "ICQ meta reply to write" << qPrintable( QString::number( m_subtype, 16 ) )
	                        << "subtype" << qPrintable( QString::number( requestSubType(), 16 ) )
	                        << "result" << qPrintable( QString::number( result, 16 ) );
	if ( result == 0x0A )
		setSuccess( 0, QString() );
	else
		setError( result, QString() );
	setTransfer( 0 );
	return true;
}

void ICQMetaWriteTask::onGo()
{
	setSequence( client()->snacSequence() );
	setRequestType( 0x07D0 );
	setRequestSubType( m_subtype );

	Buffer b;
	b.addString( m_payload );
	m_goSequence = client()->snacSequence();

	Buffer* sendBuf = addInitialData( &b );
	FLAP f = { 0x02, 0, 0 };
	SNAC s = { 0x0015, 0x0002, 0, m_goSequence };
	Transfer* t = createTransfer( f, s, sendBuf );
	send( t );
}
