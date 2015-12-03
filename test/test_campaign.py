# test_campaign.py
#  Created on: Nov 27, 2015
#      Author: pchero

import test_common

import os
import sys

def main():
    ast = test_common.Ami()
    ast.username = sys.argv[1]
    ast.password = sys.argv[2]
    if ast.conn() == False:
        print("Could not connect.")
        return 1
    
    # create campaign
    ret = ast.sendCmd("OutCampaignCreate", Name="TestCamp", Detail="TestDetail", Plan="5ad6c7d8-535c-4cd3-b3e5-83ab420dcb56", Dlma="e276d8be-a558-4546-948a-f99913a7fea2", Queue="1c8eeabb-1dbc-4b75-a688-dd5b79b5afc6")
    if ret[0]["Response"] != "Success":
        print("Couldn not pass the test_campaign. ret[%s]" % ret)
        raise "test_campaign"
    for i in range(10):
        ret = ast.recvEvt()
        if ret["Event"] == "OutCampaignCreate":
            break
    if ret["Name"] != "TestCamp" or ret["Detail"] != "TestDetail" or ret["Plan"] != "5ad6c7d8-535c-4cd3-b3e5-83ab420dcb56":
        print("Couldn not pass the test_campaign. ret[%s]" % ret)
        raise "test_campaign"
    camp_uuid = ret["Uuid"]
    
    # get campaign
    ret = ast.sendCmd("OutCampaignShow", Uuid=camp_uuid)
    flg = False
    for i in range(len(ret)):
        msg = ret[i]
        if "Uuid" not in msg:
            continue
        if msg["Uuid"] == camp_uuid:
            flg = True
            break
    if flg == False:        
        print("Couldn not pass the test_campaign. ret[%s]" % ret)
        raise "test_campaign"
    
    # delete campaign
    ret = ast.sendCmd("OutCampaignDelete", Uuid=camp_uuid)
    if ret[0]["Response"] != "Success":
        print("Couldn not pass the test_campaign. ret[%s]" % ret)
        raise "test_campaign"
    for i in range(10):
        ret = ast.recvEvt()
        if ret["Event"] == "OutCampaignDelete":
            break
    if ret["Uuid"] != camp_uuid:
        print("Couldn not pass the test_campaign. ret[%s]" % ret)
        raise "test_campaign"
    
    # get campaign
    ret = ast.sendCmd("OutCampaignShow", Uuid=camp_uuid)
    flg = True
    for i in range(len(ret)):
        msg = ret[i]
        if "Uuid" not in msg:
            continue
        if msg["Uuid"] == camp_uuid:
            flg = False
            break
    if flg == False:        
        print("Couldn not pass the test_campaign. ret[%s]" % ret)
        raise "test_campaign"

    
    return 0

if __name__ == '__main__':
    main()