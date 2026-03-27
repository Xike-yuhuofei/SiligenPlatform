#pragma once

#include "domain/system/ports/IEventPublisherPort.h"

namespace Siligen::RuntimeExecution::Contracts::System {

using IEventPublisherPort = Siligen::Domain::System::Ports::IEventPublisherPort;
using DomainEvent = Siligen::Domain::System::Ports::DomainEvent;
using EventType = Siligen::Domain::System::Ports::EventType;
using EventHandler = Siligen::Domain::System::Ports::EventHandler;

}  // namespace Siligen::RuntimeExecution::Contracts::System
