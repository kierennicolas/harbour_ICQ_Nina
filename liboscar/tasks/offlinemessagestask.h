/*
   Kopete Oscar Protocol
   offlinemessagestask.h - Offline messages handling

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

#ifndef OFFLINEMESSAGESTASK_H
#define OFFLINEMESSAGESTASK_H

#include <task.h>

class Transfer;

/**
@author Roman Jarosz

harbour_icq_nina: waits for the server's 04/17 "done" reply. If the server
does not answer (it does not support 04/10 retrieval), the task fails, and
the client then asks the ICQ way (ICQOfflineMessagesTask).
*/
class OfflineMessagesTask : public Task
{
Q_OBJECT
public:
	OfflineMessagesTask( Task* parent );

	bool forMe( const Transfer* transfer ) const Q_DECL_OVERRIDE;
	bool take( Transfer* transfer ) Q_DECL_OVERRIDE;
	void onGo() Q_DECL_OVERRIDE;

private Q_SLOTS:
	void noReply();

private:
	Oscar::DWORD m_sequence;
};

#endif

