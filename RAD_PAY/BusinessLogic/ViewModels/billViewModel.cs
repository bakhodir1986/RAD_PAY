using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class billViewModel
    {
        public long bill_id { get; set; }
        public decimal deposit { get; set; }
        public long uid { get; set; }
    }
}