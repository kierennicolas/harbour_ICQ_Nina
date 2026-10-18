/*
   Kopete Oscar Protocol
   offlinemessagestask.cpp - Offline messages handling

   Copyright (c) 2008 Roman Jarosz <kedgedev@centrum.cz>

   Kopete (c) 2008 by the Kopete developers <kopete-devel@kde.org>

   *************************************************************************
   *                                                                       *
   * This library is free software; you can redistribute it and/or         *
   * modify it under the terms of the GNU Lesser General Public            *
   * License as published by the Free Software Foundation; either          *
   * version 2 of the License, or (at your option) any later version.      *
   *                                                                       *
   *************************************************************************
*/
#include "offlinemessagestask.h"
#include <kdebug.h>
#include <QTimer>

#include "connection.h"
#include "transfer.h"

using namespace Oscar;

OfflineMessagesTask::OfflineMessagesTask( Task* parent )
	: Task( parent ), m_sequence( 0 )
{
}

bool OfflineMessagesTask::forMe( const Transfer* transfer ) const
{
	const SnacTransfer* st = dynamic_cast<const SnacTransfer*>( transfer );
	if ( !st || st->snacService() != 0x0004 || st->snacRequest() != m_sequence )
		return false;
	return st->snacSubtype() == 0x0017 || st->snacSubtype() == 0x0001;
}

bool OfflineMessagesTask::take( Transfer* transfer )
{
	if ( !forMe( transfer ) )
		return false;
	const SnacTransfer* st = dynamic_cast<const SnacTransfer*>( transfer );
	if ( st->snacSubtype() == 0x0017 )
	{
		kDebug( OSCAR_RAW_DEBUG ) << "Offline messages delivered";
		setSuccess( 0, QString() );
	}
	else
	{
		kDebug( OSCAR_RAW_DEBUG ) << "Server does not retrieve offline messages with 04/10";
		setError( 1, QString() );
	}
	return true;
}

void OfflineMessagesTask::onGo()
{
	kDebug( OSCAR_RAW_DEBUG ) << "Requesting offline messages";

	m_sequence = client()->snacSequence();
	FLAP f = { 0x02, 0, 0 };
	SNAC s = { 0x0004, 0x0010, 0x0000, m_sequence };

	Buffer* buffer = new Buffer();
	Transfer *t = createTransfer( f, s, buffer );
	send( t );

	QTimer::singleShot( 8000, this, SLOT(noReply()) );
}

void OfflineMessagesTask::noReply()
{
	// No-op when the reply already finished the task.
	setError( 2, QString() );
}
