using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class purchases_reverseViewModel
    {
        public long id { get; set; }
        public long? pid { get; set; }
        public long? aid { get; set; }
        public long? uid { get; set; }
        public string sms_code { get; set; }
        public int? status { get; set; }
        public DateTime? ts_start { get; set; }
        public DateTime? ts_confirm { get; set; }
        public string phone { get; set; }
        public string baz { get; set; }
    }
}