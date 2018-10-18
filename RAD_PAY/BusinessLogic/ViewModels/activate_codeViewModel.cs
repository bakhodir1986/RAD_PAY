using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class activate_codeViewModel
    {
        public long id { get; set; }
        public string phone { get; set; }
        public string code { get; set; }
        public DateTime? add_ts { get; set; }
        public string dev_id { get; set; }
        public bool valid { get; set; }
        public int kind { get; set; }
        public long? other_id { get; set; }
        public int? lives { get; set; }
    }
}