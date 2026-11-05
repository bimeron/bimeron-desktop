#pragma once
#include <QString>
class QObject;
namespace RepairBootstrap {
void bindOwnedChild(QObject *owner, quintptr child);
bool available();
int run(int argc, char **argv);
// Returns an owned process handle on success; caller retains it for the session.
quintptr authenticatedChild(quintptr parent, unsigned long pid);
}
