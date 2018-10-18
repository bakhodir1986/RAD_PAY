using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class user_billsViewModel
    {
        public long id { get; set; }
        public long? uid { get; set; }
        public long? uid2 { get; set; }
        public long? amount { get; set; }
        public int? merchant_id { get; set; }
        public string value { get; set; }
        public int? status { get; set; }
        public DateTime? add_ts { get; set; }
        public string comment { get; set; }
    }
}