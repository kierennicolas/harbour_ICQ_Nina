/*
  icqmetawritetask.h - one classic ICQ directory write (SNAC 0x15, request
  0x07D0 with a subtype such as 0x03EA basic info, 0x03FD more info, 0x0406
  notes) and the server's answer.

  Part of harbour_icq_nina (liboscar additions); LGPL-2.0-or-later.
*/
#ifndef ICQMETAWRITETASK_H
#define ICQMETAWRITETASK_H

#include <QByteArray>

#include "icqtask.h"

class Transfer;

class ICQMetaWriteTask : public ICQTask
{
	Q_OBJECT
public:
	ICQMetaWriteTask( Task* parent, Oscar::WORD subtype, const QByteArray& payload );

	Oscar::WORD writeSubtype() const { return m_subtype; }

	bool forMe( const Transfer* transfer ) const Q_DECL_OVERRIDE;
	bool take( Transfer* transfer ) Q_DECL_OVERRIDE;
	void onGo() Q_DECL_OVERRIDE;

private:
	Oscar::WORD m_subtype;
	QByteArray m_payload;
	Oscar::DWORD m_goSequence;
};

#endif
