# Copyright 2015 refractionPOINT
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from beach.actor import Actor
ProcessBurst = Actor.importLib( '../../analytics/StateAnalysis/descriptors', 'ProcessBurst' )
StatefulActor = Actor.importLib( '../../Detects', 'StatefulActor' )

class WinReconTools ( StatefulActor ):
    def initMachines( self, parameters ):
        self.shardingKey = 'agentid'

        reconBurst = ProcessBurst( name = 'windows_recon_burst', 
        						   procRegExp = r'.*(/|\\)((ipconfig)|(arp)|(route)|(ping)|(traceroute)|(nslookup)|(netstat)|(wmic)|(net\d?)|(whoami)|(systeminfo))\.exe',
        						   nPerBurst = 3,
        						   withinSeconds = 5 )
        
        self.addStateMachineDescriptor( reconBurst )