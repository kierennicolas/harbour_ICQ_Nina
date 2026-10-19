// Emoji for the picker and for showing messages: Sailfish OS has no colour
// emoji font, so the app brings a set of Twemoji pictures and shows message
// text as Qt "StyledText" with the emoji as inline images (and links made
// clickable).
// Part of harbour_icq_nina; GPL-3.0-or-later.
#ifndef EMOJI_H
#define EMOJI_H

#include <QHash>
#include <QObject>
#include <QVariantList>

class Emoji : public QObject
{
    Q_OBJECT
    // [{ name, items: [{ text, url }] }] for the picker.
    Q_PROPERTY(QVariantList categories READ categories NOTIFY loaded)

public:
    explicit Emoji(QObject *parent = nullptr);

    // dir holds emoji.json and the pictures.
    bool load(const QString &dir);
    QVariantList categories() const { return m_categories; }

    // Plain text as StyledText: escaped, line breaks kept, links clickable,
    // known emoji as pictures of size px.
    Q_INVOKABLE QString richText(const QString &text, int size) const;
    // The number of emoji if the text is nothing but 1 to 3 known emoji
    // (shown large), else 0.
    Q_INVOKABLE int emojiOnly(const QString &text) const;
    // The picture for one emoji ("" if not known).
    Q_INVOKABLE QString urlOf(const QString &emoji) const;

signals:
    void loaded();

private:
    // Length (in UTF-16 units) of the known emoji at pos, 0 if none; the
    // text must have no U+FE0F.
    int match(const QString &text, int pos, QString *url) const;
    QString emojiText(const QString &text, int size) const;

    QHash<QString, QString> m_urls;   // emoji text without U+FE0F -> file URL
    int m_maxLength = 0;
    QVariantList m_categories;
};

#endif
