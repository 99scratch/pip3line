/**
Released as open source by Gabriel Caudrelier

Developed by Gabriel Caudrelier, gabriel dot caudrelier at gmail dot com

https://github.com/metrodango/pip3line

Released under AGPL see LICENSE for more information
**/

#ifndef SHA384_H
#define SHA384_H

#include "transformabstract.h"

class Sha384 : public TransformAbstract
{
    Q_OBJECT
    public:
        explicit Sha384();
        ~Sha384();
        QString name() const;
        QString description() const;
        void transform(const QByteArray &input, QByteArray &output);
        bool isTwoWays();
        static const QString id;
        QString help() const;
};

#endif // SHA384_H
