﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class card_bonusViewModel
    {
        public long card_id { get; set; }
        public string number { get; set; }
        public string expire { get; set; }
        public long? xid { get; set; }
        public string name { get; set; }
        public int? tpl { get; set; }
        public string pc_token { get; set; }
        public string owner { get; set; }
        public string password { get; set; }
        public long? balance { get; set; }
        public DateTime? notify_ts { get; set; }
        public string owner_phone { get; set; }
    }
}