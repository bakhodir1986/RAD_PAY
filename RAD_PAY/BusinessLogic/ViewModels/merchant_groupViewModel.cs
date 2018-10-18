using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class merchant_groupViewModel
    {
        public int id { get; set; }
        public string name { get; set; }
        public int? position { get; set; }
        public string name_uzb { get; set; }
        public string icon_path { get; set; }
        public long? icon_id { get; set; }
    }
}