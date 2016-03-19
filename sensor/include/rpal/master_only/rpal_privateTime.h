/*
Copyright 2015 refractionPOINT

Licensed under the Apache License, Version 2.0 ( the "License" );
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http ://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#ifndef _RPAL_PRIVATE_TIME_H
#define _RPAL_PRIVATE_TIME_H

#include <rpal.h>


RPAL_DECLARE_API
( 
RU64, 
    rpal_time_getGlobal, 
);

RPAL_DECLARE_API
( 
RU64, 
    rpal_time_setGlobalOffset, 
        RU64 offset
);

RPAL_DECLARE_API
(
RU64,
    rpal_time_getGlobalLocal,
        RU64 localTs
);


#endif
