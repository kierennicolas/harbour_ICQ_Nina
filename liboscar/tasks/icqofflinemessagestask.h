/*
    harbour_icq_nina
    icqofflinemessagestask.h - ICQ offline messages over the ICQ meta family

    Retrieves messages stored while we were offline the way ICQ 2000-2003
    clients do: meta request 0x003C, one 0x0041 record per message, 0x0042 at
    the end, then 0x003E to delete them. Used when the server does not answer
    the newer 04/10 retrieval.

    This library is free software; you can redistribute it and/or modify it
    under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License, or (at your
    option) any later version.
*/
#ifndef ICQOFFLINEMESSAGESTASK_H
#define ICQOFFLINEMESSAGESTASK_H

#include "icqtask.h"
#include "oscarmessage.h"

class ICQOfflineMessagesTask : public ICQTask
{
Q_OBJECT
public:
	explicit ICQOfflineMessagesTask( Task* parent );

	bool forMe( const Transfer* transfer ) const Q_DECL_OVERRIDE;
	bool take( Transfer* transfer ) Q_DECL_OVERRIDE;
	void onGo() Q_DECL_OVERRIDE;

	// Decodes text from an old ICQ client: UTF-8 if it is valid UTF-8,
	// otherwise Windows-1252.
	static QString decodeLegacyText( const QByteArray &bytes );

Q_SIGNALS:
	void offlineMessage( const Oscar::Message &message );
	void offlineAuthRequest( const QString &contact, const QString &reason );
	void offlineAuthReply( const QString &contact, const QString &reason, bool granted );

private:
	void handleRecord( Buffer &data );
	void deleteMessages();

	Oscar::DWORD m_goSequence;
};

#endif
