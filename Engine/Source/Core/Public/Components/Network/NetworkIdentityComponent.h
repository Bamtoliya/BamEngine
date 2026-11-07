#pragma once

#include "ComponentAnnotations.h"
#include "NetworkTypes.h"
#include <cstdint>

namespace Engine
{
    STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_NETWORK_IDENTITY"), CATEGORY("CATEGORY_NETWORK"))
        struct NetworkIdentityComponent
    {
        REFLECT_BODY()

            // 네트워크 상에서 유일한 엔티티 고유 ID (0은 미할당/Invalid)
            PROPERTY(EDITABLE, DISPLAY_NAME("PROP_NETWORK_ID"))
            Network::NetId NetworkId = Network::InvalidNetId;

        // 이 엔티티의 소유 권한을 가진 클라이언트 ID (0 = Server Owned)
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_OWNER_CLIENT_ID"))
            Network::ClientId OwnerClientId = Network::ServerClientId;

        // 클라이언트 월드에 스폰하지 않는 서버 전용 엔티티 여부
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_IS_SERVER_ONLY"))
            bool IsServerOnly = false;

        // 로컬 플레이어가 직접 조작하는 엔티티인지 판별 (클라이언트 ID 비교)
        bool IsLocallyControlled(Network::ClientId localClientId) const
        {
            return (OwnerClientId != Network::ServerClientId) && (OwnerClientId == localClientId);
        }
    };
}