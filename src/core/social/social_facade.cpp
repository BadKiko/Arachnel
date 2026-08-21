#include "core_controller_impl.h"

#include "social_controller.h"

namespace arachnel::core {

void CoreController::refreshFriends()
{
    if (m_socialController)
        m_socialController->refresh();
}

void CoreController::createFriendInvite()
{
    if (m_socialController)
        m_socialController->createInviteCode();
}

void CoreController::acceptFriendInvite(const QString& code)
{
    if (m_socialController)
        m_socialController->acceptInviteCode(code);
}

void CoreController::removeFriendById(const QString& friendId)
{
    if (m_socialController)
        m_socialController->removeFriend(friendId);
}

void CoreController::renameFriendById(const QString& friendId, const QString& nickname)
{
    if (m_socialController)
        m_socialController->renameFriend(friendId, nickname);
}

void CoreController::suggestGameToFriend(const QString& friendId, const QString& entryId)
{
    if (!m_socialController)
        return;
    const QVariantMap info = entryDetails(entryId);
    m_socialController->suggestGame(friendId, entryId, info.value(QStringLiteral("title")).toString(),
                                    info.value(QStringLiteral("coverUrl")).toString());
}

} // namespace arachnel::core
